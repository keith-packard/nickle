/*
 * Copyright © 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	"ref.h"
#include	<sys/time.h>
#include	<signal.h>

volatile Bool	signalTimer;	/* Timer signal received */

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

DataType SleepQType = { SleepQMark, 0, "SleepQType" };

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
    if (thread->thread.state == ThreadSuspended)
	ThreadSetState (thread, ThreadRunning);
    return False;
}

Value
do_sleep (Value ms)
{
     ENTER ();
    int		    delta;

    if (running->thread.partial)
	RETURN (Void);
    delta = IntPart (ms, "Invalid sleep value");
    /* don't queue if instruction is aborting */
    if (aborting)
	RETURN (Void);
    TimerInsert (running, _sleepDone, delta, 0);
    /* This primitive has been partially executed */
    running->thread.partial = 1;
    SetSignalSuspend();
    RETURN (Void);
}

Value
do_millis (void)
{
    ENTER();
    RETURN(NewInt(TimeInMs()));
}

ReferencePtr	SleepingReference;

static void
_CatchAlarm (int sig)
{
    resetSignal (SIGALRM, _CatchAlarm);
    SetSignalTimer();
}

void
TimerInit (void)
{
    ENTER ();
    SleepingReference = NewReference ((void **) &sleeping);
    MemAddRoot (SleepingReference);
    catchSignal (SIGALRM, _CatchAlarm);
    EXIT ();
}
