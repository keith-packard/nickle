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
	ThreadsWakeup (m, WakeOne);
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
    if (aborting)
	RETURN (Zero);
    if (!running->thread.partial)
    {
	--s->semaphore.count;
	if (s->semaphore.count < 0)
	{
	    running->thread.partial = 1;
	    ThreadSleep (running, s, PrioritySync);
	    RETURN (One);
	}
    }
    complete = True;
    RETURN (One);
}

Value
do_Semaphore_test (Value s)
{
    ENTER ();
    if (s->semaphore.count <= 0)
	RETURN (Zero);
    RETURN (do_Semaphore_wait (s));
}

Value
do_Semaphore_signal (Value s)
{
    ENTER ();
    if (aborting)
	RETURN (Zero);
    ++s->semaphore.count;
    if (s->semaphore.count <= 0)
	ThreadsWakeup (s, WakeOne);
    complete = True;
    RETURN (One);
}

static void
SemaphoreMark (void *object)
{
}

static Bool
SemaphorePrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FilePrintf (f, "semaphore %d (%d)", av->semaphore.id, av->semaphore.count);
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
do_Semaphore_new (int n, Value *value)
{
    ENTER ();
    Value   ret;
    int	    count;
    static int	id;

    switch (n) {
    case 0:
	count = 0;
	break;
    case 1:
	count = IntPart (value[0], "Illegal initial semaphore count");
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"new: wrong number of arguments",
				2,
				NewInt (1),
				NewInt (n));
	RETURN(Zero);
    }
    ret = ALLOCATE (&SemaphoreType.data, sizeof (Semaphore));
    ret->value.tag = type_semaphore;
    ret->semaphore.count = count;
    ret->semaphore.id = ++id;
    RETURN (ret);
}

