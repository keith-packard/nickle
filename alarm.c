/*
 * $Id$
 *
 * Copyright 1996 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "nick.h"
#include "ref.h"
#include <sys/time.h>
#include <signal.h>

volatile Bool	abortTimer;	/* Timer signal received */

typedef struct _sleepQ {
    DataType	    *data;
    struct _sleepQ  *next;
    unsigned long   ms;
    unsigned long   incr;
    void	    *closure;
    TimerFunc	    func;
} SleepQ, *SleepQPtr;

SleepQPtr   sleeping;

static void
SleepQMark (void *object)
{
    SleepQPtr	sleep = object;

    MemReference (sleep->next);
    MemReference (sleep->closure);
}

DataType SleepQType = { SleepQMark, 0 };

unsigned long
TimeInMs (void)
{
    struct timeval  tv;
    struct timezone tz;

    gettimeofday (&tv, &tz);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void
_TimerSet (unsigned long when, unsigned long now)
{
    long		delta;
    struct itimerval	it, ot;
    
    delta = when - now;
    if (delta < 0)
	delta = 1;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = delta / 1000;
    it.it_value.tv_usec = (delta % 1000) * 1000;
    setitimer (ITIMER_REAL, &it, &ot);
}

static void
_TimerInsert (SleepQPtr	new, unsigned long now)
{
    SleepQPtr	*prev, s;
    for (prev = &sleeping; (s = *prev); prev = &s->next)
	if (s->ms > new->ms)
	    break;
    new->next = *prev;
    *prev = new;
    if (prev == &sleeping)
	_TimerSet (new->ms, now);
}

void
TimerInsert (void *closure, TimerFunc func,
	     unsigned long delta, unsigned long incr)
{
    ENTER ();
    unsigned long   now, ms;
    SleepQPtr	    self;

    self = ALLOCATE (&SleepQType, sizeof (SleepQ));
    now = TimeInMs ();
    ms = now + delta;
    self->next = 0;
    self->ms = ms;
    self->incr = incr;
    self->closure = closure;
    self->func = func;
    _TimerInsert (self, now);
    EXIT ();
}

void
TimerInterrupt (void)
{
    ENTER ();
    unsigned long   now;
    SleepQPtr	    s;
    
    now = TimeInMs ();
    while ((s = sleeping) && (int) (now - s->ms) >= 0)
    {
	sleeping = s->next;
	if ((*s->func) (s->closure) && s->incr)
	{
	    s->ms += s->incr;
	    _TimerInsert (s, now);
	}
    }
    if (sleeping)
	_TimerSet (sleeping->ms, now);
    EXIT ();
}

static Bool
_sleepDone (void *closure)
{
    Value   thread = closure;
    ThreadClearState (thread, ThreadSuspended);
    return False;
}

Value
Sleep (Value ms)
{
     ENTER ();
    int		    delta;

    if (running->thread.partial)
	RETURN (One);
    delta = IntPart (ms, "Invalid sleep value");
    if (exception)
	RETURN (Zero);
    TimerInsert (running, _sleepDone, delta, 0);
    /* This primitive has been partially executed */
    running->thread.partial = 1;
    aborting = True;
    exception = True;
    abortSuspend = True;
    RETURN (Zero);
}

ReferencePtr	SleepingReference;

static void
_CatchAlarm (int sig)
{
    signal (SIGALRM, _CatchAlarm);
    aborting = True;
    abortTimer = True;
}

void
TimerInit (void)
{
    ENTER ();
    SleepingReference = NewReference ((void **) &sleeping);
    MemAddRoot (SleepingReference);
    signal (SIGALRM, _CatchAlarm);
    EXIT ();
}
