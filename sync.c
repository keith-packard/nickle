/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

extern Bool complete;

Value
do_Mutex_acquire (Value m)
{
    ENTER ();
    if (m->value.tag != type_mutex)
    {
	RaiseError ("MutexAcquire: %v not mutex", m);
    }
    else if (m->mutex.owner != One)
    {
	if (m->mutex.owner == running)
	    RaiseError ("MutexAcquire: thread %v already owns mutex", running);
	else
	    ThreadSleep (running, m, PrioritySync);
    }
    else if (!aborting)
    {
	complete = True;
	m->mutex.owner = running;
    }
    RETURN (One);
}

Value
do_Mutex_try_acquire (Value m)
{
    ENTER ();
    if (m->value.tag != type_mutex)
    {
	RaiseError ("MutexAcquire: %v not mutex", m);
    }
    else if (m->mutex.owner != One)
    {
	if (m->mutex.owner == running)
	    RaiseError ("MutexAcquire: thread %v already owns mutex", running);
	else
	    RETURN (Zero);
    }
    else if (!aborting)
    {
	complete = True;
	m->mutex.owner = running;
    }
    RETURN (One);
}

Value
do_Mutex_release (Value m)
{
    ENTER ();
    if (m->value.tag != type_mutex)
	RaiseError ("MutexRelase: %v not mutex", m);
    else if (m->mutex.owner == One)
	RaiseError ("MutexRelease: mutex not owned");
    else if (m->mutex.owner != running)
	RaiseError ("MutexRelease: mutex owned by %v", m->mutex.owner);
    else if (!aborting)
    {
	complete = True;
	m->mutex.owner = One;
	ThreadsWakeup (m);
    }
    RETURN (One);
}

static void
MutexMark (void *object)
{
    Mutex   *m = object;

    MemReference (m->owner);
}

static Bool
MutexPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FilePuts (f, "mutex ");
    return Print (f, av->mutex.owner, format, base, width, prec, fill);
}

ValueType   MutexType = {
    { MutexMark, 0 },	/* base */
    {			/* binary */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ValueEqual,
	0,
	0,
    },
    {			    /* unary */
	0,
	0,
	0,
    },
    0,
    0,
    MutexPrint,
    0,
};

Value
do_Mutex_new (void)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&MutexType.data, sizeof (Mutex));
    ret->value.tag = type_mutex;
    ret->mutex.owner = One;
    RETURN (ret);
}

Value
do_Semaphore_wait (Value s)
{
    ENTER ();
    if (s->value.tag != type_semaphore)
    {
	RaiseError ("SemaphoreWait: %v not semaphore", s);
    }
    else if (s->semaphore.locked)
    {
	ThreadSleep (running, s, PrioritySync);
    }
    else if (!aborting)
    {
	complete = True;
	s->semaphore.locked = 1;
    }
    RETURN (One);
}

Value
do_Semaphore_test (Value s)
{
    ENTER ();
    if (s->value.tag != type_semaphore)
    {
	RaiseError ("SemaphoreWait: %v not semaphore", s);
    }
    else if (s->semaphore.locked)
    {
	RETURN (Zero);
    }
    else if (!aborting)
    {
	complete = True;
	s->semaphore.locked = 1;
    }
    RETURN (One);
}

Value
do_Semaphore_signal (Value s)
{
    ENTER ();
    int	old = 0;
    if (s->value.tag != type_semaphore)
    {
	RaiseError ("SemaphoreSignal: %v not semaphore", s);
	RETURN (Zero);
    }
    if (!aborting)
    {
	complete = True;
	old = s->semaphore.locked;
	s->semaphore.locked = 0;
	ThreadsWakeup (s);
    }
    RETURN (old ? One : Zero);
}

static void
SemaphoreMark (void *object)
{
}

static Bool
SemaphorePrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FilePuts (f, "semaphore");
    FilePuts (f, av->semaphore.locked ? "locked" : "unlocked");
    return True;
}

ValueType   SemaphoreType = {
    { SemaphoreMark, 0 },	/* base */
    {			/* binary */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ValueEqual,
	0,
	0,
    },
    {			    /* unary */
	0,
	0,
	0,
    },
    0,
    0,
    SemaphorePrint,
    0,
};

Value
do_Semaphore_new (void)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&SemaphoreType.data, sizeof (Semaphore));
    ret->value.tag = type_semaphore;
    ret->semaphore.locked = 0;
    RETURN (ret);
}

