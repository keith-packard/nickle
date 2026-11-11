/*
 * Copyright © 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include "nickle.h"
#include "ref.h"
#include <sys/time.h>
#include <signal.h>

volatile Bool signalTimer; /* Timer signal received */

typedef struct _sleepQ {
    DataType       *data;
    struct _sleepQ *next;
    int64_t         ms;
    int64_t         incr;
    void           *closure;
    TimerFunc       func;
} SleepQ;

static SleepQPtr sleeping;

static void
SleepQMark(void *object)
{
    SleepQPtr sleep = object;

    MemReference(sleep->next);
    MemReference(sleep->closure);
}

DataType SleepQType = { SleepQMark, 0, "SleepQType" };

int64_t
TimeInMs(void)
{
    struct timeval  tv;
    struct timezone tz;

    gettimeofday(&tv, &tz);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void
_TimerSet(int64_t when, int64_t now)
{
    int64_t          delta;
    struct itimerval it, ot;

    delta = when - now;
    /* A zero time disarms the timer rather than firing it at once */
    if (delta <= 0)
        delta = 1;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = (time_t)(delta / 1000);
    it.it_value.tv_usec = (suseconds_t)((delta % 1000) * 1000);
    setitimer(ITIMER_REAL, &it, &ot);
}

static void
_TimerInsert(SleepQPtr new, int64_t now)
{
    SleepQPtr *prev, s;
    for (prev = &sleeping; (s = *prev); prev = &s->next)
        if (s->ms > new->ms)
            break;
    new->next = *prev;
    *prev = new;
    if (prev == &sleeping)
        _TimerSet(new->ms, now);
}

/*
 * Make a timer to queue with TimerStart. It leaves the queue when it
 * fires and 'func' returns False, after which it can be started again
 */
SleepQPtr
TimerCreate(void *closure, TimerFunc func)
{
    ENTER();
    SleepQPtr self;

    self = ALLOCATE(&SleepQType, sizeof(SleepQ));
    self->next = 0;
    self->ms = 0;
    self->incr = 0;
    self->closure = closure;
    self->func = func;
    RETURN(self);
}

/*
 * Queue 'timer' to fire in 'delta' ms, then every 'incr' ms while its
 * function returns True. This allocates nothing, so it may be called
 * while collecting
 */
void
TimerStart(SleepQPtr timer, int64_t delta, int64_t incr)
{
    int64_t now = TimeInMs();

    timer->ms = now + delta;
    timer->incr = incr;
    _TimerInsert(timer, now);
}

/*
 * Remove 'timer' from the queue, if it is there, so it won't fire
 */
void
TimerCancel(SleepQPtr timer)
{
    SleepQPtr *prev;

    for (prev = &sleeping; *prev; prev = &(*prev)->next)
        if (*prev == timer) {
            *prev = timer->next;
            timer->next = 0;
            break;
        }
}

void
TimerInsert(void *closure, TimerFunc func, int64_t delta, int64_t incr)
{
    ENTER();
    TimerStart(TimerCreate(closure, func), delta, incr);
    EXIT();
}

void
TimerInterrupt(void)
{
    ENTER();
    int64_t   now;
    SleepQPtr s;

    now = TimeInMs();
    while ((s = sleeping) && now >= s->ms) {
        sleeping = s->next;
        /* A timer kept for reuse mustn't hold the rest of the queue */
        s->next = 0;
        if ((*s->func)(s->closure) && s->incr) {
            s->ms += s->incr;
            _TimerInsert(s, now);
        }
    }
    if (sleeping)
        _TimerSet(sleeping->ms, now);
    EXIT();
}

static Bool
_sleepDone(void *closure)
{
    Value thread = closure;
    if (thread->thread.state == ThreadSuspended)
        ThreadSetState(thread, ThreadRunning);
    return False;
}

Value
do_sleep(Value ms)
{
    ENTER();
    int delta;

    if (running->thread.partial)
        RETURN(Void);
    delta = IntPart(ms, "Invalid sleep value");
    /* don't queue if instruction is aborting */
    if (aborting)
        RETURN(Void);
    /* Record the timer as the thread's sleep, so a kill or signal cancels it */
    ThreadSleep(running, TimerCreate(running, _sleepDone));
    TimerStart(running->thread.sleep, delta, 0);
    /* This primitive has been partially executed */
    running->thread.partial = 1;
    RETURN(Void);
}

Value
do_millis(void)
{
    ENTER();
    RETURN(NewSignedDigitInteger(TimeInMs()));
}

static ReferencePtr SleepingReference;

static void
_CatchAlarm(int sig)
{
    (void)sig;
    resetSignal(SIGALRM, _CatchAlarm);
    SetSignalTimer();
}

void
TimerInit(void)
{
    ENTER();
    SleepingReference = NewReference((void **)&sleeping);
    MemAddRoot(SleepingReference);
    catchSignal(SIGALRM, _CatchAlarm);
    EXIT();
}
