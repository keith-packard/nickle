/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

extern Bool complete;

Value
do_Semaphore_wait (Value s)
{
    ENTER ();
    if (aborting)
	RETURN (Void);
    if (!running->thread.partial) {
	--s->semaphore.count;
	if (s->semaphore.count < 0)
	{
	    running->thread.partial = 1;
	    ThreadSleep (running, s, PrioritySync);
	    RETURN (Void);
	}
    }
    complete = True;
    RETURN (Void);
}

Value
do_Semaphore_test (Value s)
{
    ENTER ();
    if (s->semaphore.count <= 0)
	RETURN (FalseVal);
    do_Semaphore_wait (s);
    RETURN (TrueVal);
}

Value
do_Semaphore_count (Value s)
{
    ENTER();
    RETURN (NewInt(s->semaphore.count));
}

Value
do_Semaphore_signal (Value s)
{
    ENTER ();
    if (aborting)
	RETURN (Void);
    ++s->semaphore.count;
    if (s->semaphore.count <= 0)
	ThreadsWakeup (s, WakeOne);
    complete = True;
    RETURN (Void);
}

static Bool
SemaphorePrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    FilePrintf (f, "semaphore %d (%d)", av->semaphore.id, av->semaphore.count);
    return True;
}

ValueRep   SemaphoreRep = {
    { 0, 0, "SemaphoreRep" },   /* base */
    rep_semaphore,	    /* tag */
    {			    /* binary */
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("new: wrong number of arguments"),
				NewInt (1), NewInt (n));
	RETURN(Void);
    }
    ret = ALLOCATE (&SemaphoreRep.data, sizeof (Semaphore));
    ret->semaphore.count = count;
    ret->semaphore.id = ++id;
    RETURN (ret);
}

