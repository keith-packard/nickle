/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	"ref.h"

Value   running;
Value   stopped;
int	runnable;
Bool	signalException;

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
	if (t->thread.priority < thread->thread.priority)
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
    SetSignalSuspend ();
}

void
ThreadStepped (Value thread)
{
    Value   t;
    
    if ((t = thread->thread.next) &&
        thread->thread.priority <= t->thread.priority)
    {
        _ThreadRemove (thread);
        _ThreadInsert (thread);
    }
}

void
ThreadsWakeup (Value sleep, WakeKind kind)
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
	    if (kind == WakeOne)
		break;
	}
    }
}

void
ThreadFinish (Value thread)
{
    ThreadSetState (thread, ThreadFinished);
    ThreadsWakeup (thread, WakeAll);
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
    if (!ValueIsThread(target))
    {
	RaiseError ("Join needs thread argument");
	RETURN (Zero);
    }
    if ((target->thread.state & ThreadFinished) == 0)
    {
	ThreadSleep (running, target, PrioritySync);
	RETURN (Zero);
    }
    RETURN (target->thread.context.value);
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
    }
}

Value
do_Thread_list (void)
{
    Value   t;

    for (t = running; t; t = t->thread.next)
    {
	FilePrintf (FileStdout, "\t%%%d", t->thread.id);
	ThreadListState (t);
	FileOutput (FileStdout, '\n');
    }
    for (t = stopped; t; t = t->thread.next)
    {
	FilePrintf (FileStdout, "\t%%%d", t->thread.id);
	ThreadListState (t);
	if (t->thread.sleep)
	    FilePrintf (FileStdout, " %v", t->thread.sleep);
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
    if (aborting)
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
    if (!ValueIsThread(thread))
    {
	RaiseError ("SetPriority: %v not a thread", thread);
	RETURN (Zero);
    }
    i = IntPart (priority, "Invalid thread priority");
    if (aborting)
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
    if (!ValueIsThread(thread))
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
    
    if (!ValueIsThread(thread))
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
	thread = lookupVar (0, "thread");
	if (!ValueIsThread(thread))
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

void
TraceFunction (FramePtr frame, CodePtr code, ExprPtr name)
{
    int		    fe;
    
    if (name)
	PrettyExpr (FileStdout, name, -1, 0, False);
    else
	FilePuts (FileStdout, "<anonymous>");
    FilePuts (FileStdout, " (");
    for (fe = 0; fe < code->base.argc; fe++)
    {
	if (fe)
	    FilePuts (FileStdout, ", ");
	FilePrintf (FileStdout, "%v", BoxValue (frame->frame, fe));
    }
    FilePuts (FileStdout, ")\n");
}

void
TraceFrame (FramePtr frame, InstPtr pc)
{
    ENTER ();
    int		max;
    CodePtr	code;

    PrettyStat (FileStdout, pc->base.stat, False);
    for (max = 20; frame && max--; frame = frame->previous)
    {
	code = frame->function->func.code;
	TraceFunction (frame, code, code->base.name);
	PrettyStat (FileStdout, frame->savePc->base.stat, False);
    }
    EXIT ();
}

#ifdef DEBUG_JUMP
static void
TraceIndent (int indent)
{
    while (indent--)
	FilePuts (FileStdout, "    ");
}
#endif

Value
do_Thread_trace (int n, Value *p)
{
    ENTER ();
    Value	v;
    FramePtr	frame;
    InstPtr	pc;
    
    if (n == 0)
	v = lookupVar (0, "cont");
    else
	v = *p;
    switch (ValueTag(v)) {
    case type_thread:
    case type_continuation:
	frame = v->continuation.context.frame;
	pc = v->continuation.context.pc;
	break;
    default:
	if (n == 0)
	    RaiseError ("trace: no default continuation");
	else
	    RaiseError ("trace: %v neither continuation nor thread", v);
	RETURN (Zero);
    }
    TraceFrame (frame, pc);
    RETURN(One);
}

static void
ThreadMark (void *object)
{
    ThreadPtr	thread = object;

    ContextMark (&thread->context);
    MemReference (thread->jump);
    MemReference (thread->sleep);
    MemReference (thread->next);
}

static Bool
ThreadPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FilePrintf (f, "%%%d", av->thread.id);
    return True;
}

ValueType    ThreadType = {
    { ThreadMark, 0 },	/* base */
    type_thread,	/* tag */
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
    
    ret->thread.jump = 0;
    ret->thread.state = ThreadRunning;
    ret->thread.priority = 0;
    ret->thread.sleep = 0;
    ret->thread.id = ++ThreadId;
    ret->thread.partial = 0;
    ret->thread.next = 0;
    
    ContextInit (&ret->thread.context);
    ret->thread.context.pc = ObjCode (code, 0);
    
    complete = True;
    if (code->error)
	ret->thread.state = ThreadFinished;
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

    ContextMark (&continuation->context);
}

static Bool
ContinuationPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FilePuts (f, "continutation");
    return True;
}

ValueType    ContinuationType = {
    { ContinuationMark, 0 },	/* base */
    type_continuation,		/* tag */
    {				/* binary */
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
    {				/* unary */
	0,
	0,
	0,
    },
    0,
    0,
    ContinuationPrint,
    0,
};

Value
NewContinuation (ContextPtr context, InstPtr pc)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&ContinuationType.data, sizeof (Continuation));
    ret->continuation.context.pc = pc;
    ContextSet (&ret->continuation.context, context);
    RETURN (ret);
}

#ifdef DEBUG_JUMP

void
ContextTrace (char *where, Context *context, int indent)
{
    int	    s;
    StackObject	*stack = context->stack;
    CatchPtr	catches = context->catches;
    TwixtPtr	twixts = context->twixts;
    InstPtr	pc = context->pc;
    
    TraceIndent (indent);
    FilePuts (FileStdout, "*** ");
    FilePuts (FileStdout, where);
    FilePuts (FileStdout, " ***\n");
    TraceIndent (indent);
    FilePuts (FileStdout, "stack:     ");
    for (s = 0; STACK_TOP(stack) + (s) < STACK_MAX(stack); s++)
    {
	if (s)
	    FilePuts (FileStdout, ", ");
	FilePrintf (FileStdout, "%v", STACK_ELT(stack, s));
    }
    FilePuts (FileStdout, "\n");
    TraceIndent (indent);
    FilePuts (FileStdout, "catches:   ");
    for (s = 0; catches; catches = catches->previous, s++)
    {
	if (s)
	    FilePuts (FileStdout, ", ");
	FilePrintf (FileStdout, "%A", catches->exception->symbol.name);
    }
    FilePuts (FileStdout, "\n");
    TraceIndent (indent);
    FilePuts (FileStdout, "statement: ");
    PrettyStat (FileStdout, pc->base.stat, False);
    for (s = 0; twixts; twixts = twixts->context.twixts, s++)
    {
	ContextTrace ("twixt", &twixts->context, indent+1);
    }
}
#endif

InstPtr
ContextSet (ContextPtr dst, ContextPtr src)
{
    ENTER ();
    dst->obj = src->obj;
    dst->frame = src->frame;
    dst->value = src->value;
    dst->catches = src->catches;
    dst->twixts = src->twixts;
    dst->stack = 0;
    /* last, to make sure remaining entries are initialized before any GC */
    dst->stack = StackCopy (src->stack);
    RETURN (src->pc);
}

void
ContextInit (ContextPtr dst)
{
    dst->pc = 0;
    dst->obj = 0;
    dst->frame = 0;
    dst->value = Zero;
    dst->catches = 0;
    dst->twixts = 0;
    dst->stack = 0;
    dst->stack = StackCreate ();
}

#include <assert.h>

void
ContextMark (ContextPtr	context)
{
    assert (ObjCode (context->obj, 0) <= context->pc &&
	    context->pc <= ObjCode (context->obj, ObjLast(context->obj))); 
    MemReference (context->obj);
    MemReference (context->frame);
    MemReference (context->stack);
    MemReference (context->value);
    MemReference (context->catches);
    MemReference (context->twixts);
}

static void
MarkJump (void *object)
{
    JumpPtr jump = object;

    MemReference (jump->enter);
    MemReference (jump->entering);
    MemReference (jump->leave);
    MemReference (jump->parent);
    MemReference (jump->continuation);
    MemReference (jump->ret);
}

DataType    JumpType = { MarkJump, 0 };

JumpPtr
NewJump (TwixtPtr leave, TwixtPtr enter, TwixtPtr parent,
	 Value continuation, Value ret)
{
    ENTER ();
    JumpPtr jump;

    jump = ALLOCATE (&JumpType, sizeof (Jump));
    jump->leave = leave;
    jump->enter = enter;
    jump->entering = TwixtNext (parent, enter);
    jump->parent = parent;
    jump->continuation = continuation;
    jump->ret = ret;
    RETURN (jump);
}

Value
ContinuationJump (Value thread, Value continuation, Value ret, InstPtr *next)
{
    ENTER ();
#ifdef DEBUG_JUMP
    ContextTrace ("ContinuationJump from", &thread->thread.context, 1);
    ContextTrace ("ContinuationJump to", &continuation->continuation.context, 1);
#endif
    /*
     * If there are twixt enter or leave blocks to execute, build a Jump
     * that walks them and then resets the context.
     *
     * Otherwise, just jump
     */
    if (thread->thread.context.twixts != continuation->continuation.context.twixts)
	*next = JumpStart (thread, 
			   thread->thread.context.twixts,
			   continuation->continuation.context.twixts,
			   continuation, ret);
    else
	*next = ContextSet (&thread->thread.context, 
			    &continuation->continuation.context);
    RETURN (ret);
}

/*
 * Figure out where to go next in a longjmp through twixts
 */
Value
JumpContinue (Value thread, InstPtr *next)
{
    ENTER ();
    JumpPtr	jump = thread->thread.jump;
    TwixtPtr	twixt;
    
    if (jump->leave)
    {
	/*
	 * Going up
	 */
	twixt = jump->leave;
	jump->leave = twixt->context.twixts;
	/*
	 * Matching element of the twixt chain, next time start
	 * back down the other side
	 */
	if (jump->leave == jump->parent)
	    jump->leave = 0;
	ContextSet (&thread->thread.context, &twixt->context);
	*next = twixt->leave;
    }
    else if (jump->entering)
    {
	/*
	 * Going down
	 */
	twixt = jump->entering;
	jump->entering = TwixtNext (jump->entering, jump->enter);
	*next = ContextSet (&thread->thread.context, &twixt->context);
    }
    else
    {
	/*
	 * All done, set to final context and drop the jump object
	 */
	running->thread.jump = 0;
	*next = ContextSet (&thread->thread.context, 
			    &jump->continuation->continuation.context);
    }
    RETURN (jump->ret);
}

/*
 * Build a Jump that threads through all of the necessary twixt blocks
 * ending up at 'continuation' returning 'ret'
 */
InstPtr
JumpStart (Value thread, TwixtPtr leave, TwixtPtr enter, 
	   Value continuation, Value ret)
{
    ENTER ();
    int	diff;
    TwixtPtr	leave_parent, enter_parent, parent;
    InstPtr	next;

    leave_parent = leave;
    enter_parent = enter;
    /*
     * Make both lists the same length.  Note that either can be empty
     */
    diff = TwixtDepth (leave_parent) - TwixtDepth (enter_parent);
    if (diff >= 0)
	while (diff-- > 0)
	    leave_parent = leave_parent->context.twixts;
    else
	while (diff++ < 0)
	    enter_parent = enter_parent->context.twixts;
    /*
     * Now find the common parent
     */
    while (leave_parent != enter_parent)
    {
	leave_parent = leave_parent->context.twixts;
	enter_parent = enter_parent->context.twixts;
    }

    parent = enter_parent;
    /*
     * Build a data structure to get from leave to enter via parent
     */
    thread->thread.jump = NewJump (leave, enter, parent, continuation, ret);
    /*
     * Don't need the jump return value yet; we're off to the twixt
     * blocks; after that, the return value will get retrieved by the
     * final OpLeaveDone or OpEnterDone instruction
     */
    (void) JumpContinue (thread, &next);
    RETURN (next);
}


/*
 * It is necessary that SetJump and LongJump have the same number
 * of arguments -- the arguments pushed by SetJump will have to be
 * popped when LongJump executes.  If this is not so, the stack copy
 * created here should be adjusted to account for this difference
 */
Value
do_setjmp (Value continuation_ref, Value ret)
{
    ENTER ();
    Value	continuation;
    
    if (!ValueIsRef(continuation_ref))
    {
	RaiseError ("setjump: not a reference %v", continuation_ref);
	RETURN (Zero);
    }
    continuation = NewContinuation (&running->thread.context,
				    running->thread.context.pc + 1);
    /*
     * Adjust stack for set jump return
     */
    STACK_DROP (continuation->continuation.context.stack, 2);
    RefValueSet (continuation_ref, continuation);
#ifdef DEBUG_JUMP
    ContextTrace ("do_setjmp", &continuation->continuation.context, 1);
#endif
    RETURN (ret);
}

Value
do_longjmp (InstPtr *next, Value continuation, Value ret)
{
    ENTER ();

    if (!running)
	RETURN (Zero);
    if (!ValueIsContinuation(continuation))
    {
	RaiseError ("longjump: not a continuation %v", continuation);
	RETURN (Zero);
    }
    RETURN (ContinuationJump (running, continuation, ret, next));
}

static void
MarkCatch (void *object)
{
    CatchPtr	catch = object;

    MemReference (catch->previous);
    MemReference (catch->continuation);
    MemReference (catch->exception);
}

DataType    CatchType = { MarkCatch, 0 };

CatchPtr
NewCatch (CatchPtr previous, Value continuation, SymbolPtr except)
{
    ENTER();
    CatchPtr	catch;

    catch = ALLOCATE (&CatchType, sizeof (Catch));
    catch->previous = previous;
    catch->continuation = continuation;
    catch->exception = except;
    RETURN (catch);
}

static void
TwixtMark (void *object)
{
    TwixtPtr	twixt = object;

    ContextMark (&twixt->context);
    MemReference (twixt->leave);
}

DataType    TwixtType = { TwixtMark, 0 };

TwixtPtr
NewTwixt (ContextPtr	context,
	  InstPtr	enter,
	  InstPtr	leave)
{
    ENTER ();
    TwixtPtr	twixt;

    twixt = ALLOCATE (&TwixtType, sizeof (Twixt));
    twixt->context.pc = enter;
    twixt->leave = leave;
    if (context->twixts)
	twixt->depth = context->twixts->depth + 1;
    else
	twixt->depth = 1;
    ContextSet (&twixt->context, context);
    RETURN (twixt);
}

/*
 * Twixts are chained deepest first.  Walking
 * down the list is a bit of work
 */

TwixtPtr
TwixtNext (TwixtPtr twixt, TwixtPtr last)
{
    if (last == twixt)
	return 0;
    while (last->context.twixts != twixt)
	last = last->context.twixts;
    return last;
}

void
RaiseException (Value thread, SymbolPtr except, Value args, InstPtr *next)
{
    ENTER ();
    Bool	caught = False;
    CatchPtr	catch;
    
    for (catch = thread->thread.context.catches; catch; catch = catch->previous)
	if (catch->exception == except)
	{
	    ContinuationJump (thread, catch->continuation, args, next);
	    caught = True;
	    break;
	}
    if (!caught)
    {
	int	i;
	ExprPtr	stat = thread->thread.context.pc->base.stat;
	
	if (stat->base.file)
	    PrintError ("Unhandled exception \"%A\" at %A:%d\n", 
			except->symbol.name, stat->base.file, stat->base.line);
	else
	    PrintError ("Unhandled exception \"%A\"\n", 
			except->symbol.name);
	if (args)
	{
	    for (i = args->array.ents - 1; i >= 0; i--)
		PrintError ("\t%v\n", BoxValueGet (args->array.values, i));
	}
	SetSignalError ();
    }
    EXIT ();
}

SymbolPtr	    standardExceptions[_num_standard_exceptions];
StandardException   standardException;
Value		    standardExceptionArgs;
ReferencePtr	    standardExceptionArgsRef;

void
RegisterStandardException (StandardException	se,
			   SymbolPtr		sym)
{
    ENTER ();
    standardExceptions[se] = sym;
    MemAddRoot (sym);
    if (!standardExceptionArgsRef)
    {
	standardExceptionArgsRef = NewReference ((void **) &standardExceptionArgs);
	MemAddRoot (standardExceptionArgsRef);
    }
    EXIT ();
}

void
RaiseStandardException (StandardException   se,
			char		    *string,
			int		    argc,
			...)
{
    ENTER ();
    Value	args;
    int		i;
    va_list	va;
    
    va_start (va, argc);
    i = argc + 1;
    args = NewArray (False, typesPoly, 1, &i);
    BoxValueSet (args->array.values, 0, NewStrString (string));
    if (argc)
	for (i = 0; i < argc; i++)
	    BoxValueSet (args->array.values, i+1, va_arg (va, Value));
    standardException = se;
    standardExceptionArgs = args;
    SetSignalException ();
    EXIT ();
}

void
JumpStandardException (Value thread, InstPtr *next)
{
    ENTER ();
    SymbolPtr		except = standardExceptions[standardException];
    
    aborting = False;
    if (except)
	RaiseException (thread, except, standardExceptionArgs, next);
    standardException = exception_none;
    standardExceptionArgs = 0;
    EXIT ();
}
