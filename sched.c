/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"nickle.h"
#include	"ref.h"

Value   running;
Value   stopped;
int	runnable;

extern void dumpSleep (void), dumpThreads (void);

static void
_ThreadInsert (Value thread)
{
    Value	*prev, t;

    if (Runnable (thread))
	++runnable;
    if (thread->thread.state == ThreadRunning)
	prev = &running;
    else if (thread->thread.state & ThreadFinished)
	return;
    else
	prev = &stopped;
    for (; (t = *prev); prev = &t->thread.next)
	if (t->thread.priority <= thread->thread.priority)
	    break;
    thread->thread.next = t;
    *prev = thread;
}

static void
_ThreadRemove (Value thread)
{
    Value	*prev;

    if (Runnable (thread))
	--runnable;
    if (thread->thread.state == ThreadRunning)
	prev = &running;
    else if (thread->thread.state & ThreadFinished)
	return;
    else
	prev = &stopped;
    for (; *prev != thread; prev = &(*prev)->thread.next);
    *prev = thread->thread.next;
}

void
ThreadSetState (Value thread, ThreadState state)
{
    _ThreadRemove (thread);
    thread->thread.state |= state;
    _ThreadInsert (thread);
}

void
ThreadClearState (Value thread, ThreadState state)
{
    _ThreadRemove (thread);
    thread->thread.state &= ~state;
    _ThreadInsert (thread);
}

void
ThreadSleep (Value thread, Value sleep, int priority)
{
    thread->thread.priority = priority;
    thread->thread.sleep = sleep;
    aborting = True;
    exception = True;
    abortSuspend = True;
}

void
ThreadStepped (Value thread)
{
    Value   t;
    
    if (thread == running &&
	thread->thread.priority > PriorityMin)
    {
	thread->thread.priority--;
	if ((t = thread->thread.next) &&
	    thread->thread.priority < t->thread.priority)
	{
	    _ThreadRemove (thread);
	    _ThreadInsert (thread);
	}
    }
}

void
ThreadsWakeup (Value sleep)
{
    Value	thread, next;

    for (thread = stopped; thread; thread = next)
    {
	next = thread->thread.next;
	if ((thread->thread.state & ThreadSuspended) && 
	    thread->thread.sleep == sleep)
	{
	    thread->thread.sleep = 0;
	    ThreadClearState (thread, ThreadSuspended);
	}
    }
}

void
ThreadFinish (Value thread)
{
    ThreadSetState (thread, ThreadFinished);
    ThreadsWakeup (thread);
}

void
ThreadsInterrupt (void)
{
    Value   thread, next;
    Value   t;

    if (running)
	t = running;
    else
	t = stopped;
    for (thread = stopped; thread; thread = next)
    {
	next = thread->thread.next;
	if (Runnable (thread))
	    --runnable;
	thread->thread.state |= ThreadInterrupted;
    }
    for (thread = running; thread; thread = next)
    {
	next = thread->thread.next;
	ThreadSetState (thread, ThreadInterrupted);
    }
    if (t)
	DebugSetFrame (t, 0);
}

static int
ThreadContinue (void)
{
    Value   thread, next;
    int	    n;

    n = 0;
    for (thread = stopped; thread; thread = next)
    {
	next = thread->thread.next;

	if (thread->thread.state & ThreadInterrupted)
	{
	    n++;
	    ThreadClearState (thread, ThreadInterrupted);
	}
    }
    return n;
}
	    
Value
do_Thread_join (Value target)
{
    ENTER ();
    if (target->value.tag != type_thread)
    {
	RaiseError ("Join needs thread argument");
	RETURN (Zero);
    }
    if ((target->thread.state & ThreadFinished) == 0)
    {
	ThreadSleep (running, target, PrioritySync);
	RETURN (Zero);
    }
    RETURN (target->thread.v);
}

static void
ThreadListState (Value thread)
{
    int	state = thread->thread.state;
    
    if (state == ThreadRunning)
	FilePuts (FileStdout, " running");
    else
    {
	if (state & ThreadSuspended)
	    FilePuts (FileStdout, " suspended");
	if (state & ThreadInterrupted)
	    FilePuts (FileStdout, " interrupted");
	if (state & ThreadFinished)
	    FilePuts (FileStdout, " finished");
	if (state & ThreadError)
	    FilePuts (FileStdout, " error");
    }
}

Value
do_Thread_list (void)
{
    Value   t;

    for (t = running; t; t = t->thread.next)
    {
	FilePuts (FileStdout, "\t%");
	FilePutInt (FileStdout, t->thread.id);
	ThreadListState (t);
	FileOutput (FileStdout, '\n');
    }
    for (t = stopped; t; t = t->thread.next)
    {
	FilePuts (FileStdout, "\t%");
	FilePutInt (FileStdout, t->thread.id);
	ThreadListState (t);
	if (t->thread.sleep)
	{
	    FileOutput (FileStdout, ' ');
	    print (FileStdout, t->thread.sleep);
	}
	FileOutput (FileStdout, '\n');
    }
    return Zero;
}

Value
do_Thread_id_to_thread (Value id)
{
    ENTER ();
    int	i;
    Value   t;

    i = IntPart (id, "Invalid thread id");
    if (exception)
	RETURN (Zero);
    for (t = running; t; t = t->thread.next)
	if (t->thread.id == i)
	    RETURN (t);
    for (t = stopped; t; t = t->thread.next)
	if (t->thread.id == i)
	    RETURN (t);
    RETURN (Zero);
}

Value
do_Thread_current (void)
{
    ENTER ();
    Value   ret;
    if (running)
	ret = running;
    else
	ret = Zero;
    RETURN (ret);
}

Value
do_Thread_set_priority (Value thread, Value priority)
{
    ENTER ();
    int	    i;
    if (thread->value.tag != type_thread)
    {
	RaiseError ("SetPriority: %v not a thread", thread);
	RETURN (Zero);
    }
    i = IntPart (priority, "Invalid thread priority");
    if (exception)
	RETURN (Zero);
    if (i != thread->thread.priority)
    {
	_ThreadRemove (thread);
	thread->thread.priority = i;
	_ThreadInsert (thread);
    }
    RETURN (NewInt (thread->thread.priority));
}

Value
do_Thread_get_priority (Value thread)
{
    ENTER ();
    if (thread->value.tag != type_thread)
    {
	RaiseError ("GetPriority: %v not a thread", thread);
	RETURN (Zero);
    }
    RETURN (NewInt (thread->thread.priority));
}

Value
do_Thread_cont (void)
{
    int	    n;
    
    n = ThreadContinue ();
    return NewInt (n);
}

static int
KillThread (Value thread)
{
    int	ret;
    
    if (thread->value.tag != type_thread)
    {
	RaiseError ("Kill: %v not a thread", thread);
	return 0;
    }
    if (thread->thread.state & ThreadFinished)
	ret = 0;
    else
	ret = 1;
    ThreadFinish (thread);
    return ret;
}

Value
do_Thread_kill (int n, Value *p)
{
    ENTER ();
    Value   thread;
    int	    ret = 0;

    if (n == 0)
    {
	thread = lookupVar ("thread");
	if (thread->value.tag != type_thread)
	    RaiseError ("Kill: no default thread");
	else
	    ret = KillThread (thread);
    }
    else
    {
	while (n--)
	    ret += KillThread (*p++);
    }
    RETURN (NewInt (ret));
}

static void
TraceFunction (Value thread, FramePtr frame, CodePtr code, Atom name)
{
    int		    fe;
    
    FilePuts (FileStdout, name ? AtomName (name) : "<anonymous>");
    FilePuts (FileStdout, " (");
    for (fe = 0; fe < code->base.argc; fe++)
    {
	if (fe)
	    FilePuts (FileStdout, ", ");
	print (FileStdout, BoxValue (frame->frame, fe));
    }
    FilePuts (FileStdout, ")\n");
}

Value
do_Thread_trace (int n, Value *p)
{
    ENTER ();
    FramePtr	f;
    int		max;
    CodePtr	code;
    Value	thread;
    
    if (n == 0)
	thread = lookupVar ("thread");
    else
	thread = *p;
    if (thread->value.tag != type_thread)
    {
	if (n == 0)
	    RaiseError ("Trace: no default thread");
	else
	    RaiseError ("Trace: %v not a thread", thread);
	RETURN (Zero);
    }
    PrintStat (FileStdout, thread->thread.pc->base.stat, False);
    for (f = thread->thread.frame, max = 20; f && max--; f = f->previous)
    {
	code = f->function->func.code;
	TraceFunction (thread, f, code, code->base.name);
	PrintStat (FileStdout, f->savePc->base.stat, False);
    }
    RETURN(One);
}

static void
ThreadMark (void *object)
{
    ThreadPtr	thread = object;

    MemReference (thread->v);
    MemReference (thread->stack);
    MemReference (thread->frame);
    MemReference (thread->code);
    MemReference (thread->sleep);
    MemReference (thread->next);
}

static Bool
ThreadPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FileOutput (f, '%');
    FilePutInt (f, av->thread.id);
    return True;
}

ValueType    ThreadType = {
    { ThreadMark, 0 },	/* base */
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
    ThreadPrint,
    0,
};
    
static int  ThreadId;

Value
NewThread (FramePtr frame, ObjPtr code)
{
    ENTER ();
    Value ret;

    ret = ALLOCATE (&ThreadType.data, sizeof (Thread));
    ret->value.tag = type_thread;
    ret->thread.v = Zero;
    ret->thread.stack = StackCreate ();
    ret->thread.pc = ObjCode (code, 0);
    ret->thread.code = code;
    ret->thread.frame = frame;
    ret->thread.state = ThreadRunning;
    ret->thread.priority = 0;
    ret->thread.sleep = 0;
    ret->thread.id = ++ThreadId;
    ret->thread.partial = 0;
    complete = True;
    if (code->errors)
	ret->thread.state = ThreadError;
    _ThreadInsert (ret);
    RETURN (ret);
}

ReferencePtr	RunningReference, StoppedReference;

void
ThreadInit (void)
{
    ENTER ();
    RunningReference = NewReference ((void **) &running);
    MemAddRoot (RunningReference);
    StoppedReference = NewReference ((void **) &stopped);
    MemAddRoot (StoppedReference);
    EXIT ();
}

static void
ContinuationMark (void *object)
{
    ContinuationPtr continuation = object;

    MemReference (continuation->frame);
}

static Bool
ContinuationPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FilePuts (f, "continutation");
    return True;
}

ValueType    ContinuationType = {
    { ContinuationMark, 0 },	/* base */
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
    ContinuationPrint,
    0,
};

static Value
NewContinuation (FramePtr frame, InstPtr pc, StackObject *stack)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&ContinuationType.data, sizeof (Continuation));
    ret->value.tag = type_continuation;
    ret->continuation.frame = frame;
    ret->continuation.pc = pc;
    ret->continuation.stack = stack;
    RETURN (ret);
}

/*
 * It is necessary that SetJump and LongJump have the same number
 * of arguments -- the arguments pushed by SetJump will have to be
 * popped when LongJump executes.  If this is not so, the stack copy
 * created here should be adjusted to account for this difference
 */
Value
do_set_jump (InstPtr *next, Value continuation_ref, Value ret)
{
    ENTER ();
    Value   continuation;
    
    if (continuation_ref->value.tag != type_ref)
    {
	RaiseError ("setjump: not a reference %v", continuation_ref);
	RETURN (Zero);
    }
    continuation = NewContinuation (running->thread.frame, *next,
				    StackCopy (running->thread.stack));
    RefValue (continuation_ref) = continuation;
    RETURN (ret);
}

Value
do_long_jump (InstPtr *next, Value continuation, Value ret)
{
    ENTER ();

    if (!running)
	RETURN (Zero);
    if (continuation->value.tag != type_continuation)
    {
	RaiseError ("longjump: not a continuation %v", continuation);
	RETURN (Zero);
    }
    running->thread.frame = continuation->continuation.frame;
    running->thread.stack = StackCopy (continuation->continuation.stack);
    *next = continuation->continuation.pc;
    RETURN (ret);
}
