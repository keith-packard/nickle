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

extern Bool complete;

Value
MutexAcquire (Value m)
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
MutexTryAcquire (Value m)
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
MutexRelease (Value m)
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

Bool
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
	0,
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
NewMutex (void)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&MutexType.data, sizeof (Mutex));
    ret->value.tag = type_mutex;
    ret->mutex.owner = One;
    RETURN (ret);
}

Value
SemaphoreWait (Value s)
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
SemaphoreTest (Value s)
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
SemaphoreSignal (Value s)
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

Bool
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
	0,
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
NewSemaphore (void)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&SemaphoreType.data, sizeof (Semaphore));
    ret->value.tag = type_semaphore;
    ret->semaphore.locked = 0;
    RETURN (ret);
}

