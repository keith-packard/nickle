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
    if (thread->value.tag != type_thread)
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
	thread = lookupVar (0, "thread");
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

void
TraceContinuation (char	    *where,
		   FramePtr frame,
		   StackObject *stack,
		   CatchPtr catches,
		   TwixtPtr twixts,
		   InstPtr  pc,
		   int	    indent)
{
    int	    s;
    
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
    for (s = 0; twixts; twixts = twixts->previous, s++)
    {
	TraceContinuation ("twixt",
			   twixts->frame,
			   twixts->stack,
			   twixts->catches,
			   0,
			   twixts->enter,
			   indent+1);
    }
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
    switch (v->value.tag) {
    case type_thread:
	frame = v->thread.frame;
	pc = v->thread.pc;
	break;
    case type_continuation:
	frame = v->continuation.frame;
	pc = v->continuation.pc;
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

    MemReference (thread->v);
    MemReference (thread->stack);
    MemReference (thread->frame);
    MemReference (thread->code);
    MemReference (thread->catches);
    MemReference (thread->twixts);
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

    MemReference (continuation->value.type);
    MemReference (continuation->frame);
    MemReference (continuation->stack);
    MemReference (continuation->catches);
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
    ContinuationPrint,
    0,
};

Value
NewContinuation (FramePtr frame, InstPtr pc, 
		 StackObject *stack, CatchPtr catches,
		 TwixtPtr twixts)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&ContinuationType.data, sizeof (Continuation));
    ret->value.tag = type_continuation;
    ret->continuation.frame = frame;
    ret->continuation.pc = pc;
    ret->continuation.stack = stack;
    ret->continuation.catches = catches;
    ret->continuation.twixts = twixts;
    RETURN (ret);
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
    MemReference (jump->args);
}

DataType    JumpType = { MarkJump, 0 };

JumpPtr
NewJump (TwixtPtr leave, TwixtPtr enter, 
	 TwixtPtr parent, Value continuation, Value ret)
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
    jump->args = 0;
    RETURN (jump);
}

#ifdef DEBUG_JUMP
void
ContinuationTrace (char	*where, Value continuation)
{
    TraceContinuation (where,
		       continuation->continuation.frame,
		       continuation->continuation.stack,
		       continuation->continuation.catches,
		       continuation->continuation.twixts,
		       continuation->continuation.pc,
		       1);
}
#endif

void
ContinuationJump (Value thread, Value continuation, InstPtr *next)
{
#ifdef DEBUG_JUMP
    ContinuationTrace ("ContinuationJump", continuation);
#endif
    running->thread.frame = continuation->continuation.frame;
    running->thread.stack = StackCopy (continuation->continuation.stack);
    running->thread.catches = continuation->continuation.catches;
    running->thread.twixts = continuation->continuation.twixts;
    running->thread.jump = 0;
    *next = continuation->continuation.pc;
}

void
ContinuationArgs (Value thread, BoxPtr args)
{
    if (thread->thread.jump)
	thread->thread.jump->args = args;
    else
    {
	int	    i = args->nvalues;
	while (--i >= 0)
	    STACK_PUSH (thread->thread.stack, BoxValue (args, i));
	STACK_PUSH (thread->thread.stack, NewInt (args->nvalues));
    }
}

/*
 * Figure out where to go next in a longjmp through twixts
 */
Value
JumpContinuation (JumpPtr jump, InstPtr *next)
{
    ENTER ();
    TwixtPtr	twixt;
    
    if (jump->leave)
    {
	twixt = jump->leave;
	jump->leave = twixt->previous;
	if (jump->leave == jump->parent)
	    jump->leave = 0;
	TwixtJump (running, twixt, False, next);
    }
    else if (jump->entering)
    {
	twixt = jump->entering;
	jump->entering = TwixtNext (jump->entering, jump->enter);
	TwixtJump (running, twixt, True, next);
    }
    else
    {
	ContinuationJump (running, jump->continuation, next);
	if (jump->args)
	    ContinuationArgs (running, jump->args);
    }
    RETURN (jump->ret);
}

JumpPtr
JumpBuild (TwixtPtr leave, TwixtPtr enter, 
	   Value continuation, Value ret, InstPtr  *next)
{
    ENTER ();
    int	diff;
    TwixtPtr	leave_parent, enter_parent, parent;
    JumpPtr	jump = 0;

    leave_parent = leave;
    enter_parent = enter;
    /*
     * Make both lists the same length 
     */
    diff = TwixtDepth (leave_parent) - TwixtDepth (enter_parent);
    if (diff >= 0)
	while (diff-- > 0)
	    leave_parent = leave_parent->previous;
    else
	while (diff++ < 0)
	    enter_parent = enter_parent->previous;
    /*
     * Now find the common parent
     */
    while (leave_parent != enter_parent)
    {
	leave_parent = leave_parent->previous;
	enter_parent = enter_parent->previous;
    }

    parent = enter_parent;
    /*
     * Build a data structure to get from leave to enter via parent
     */
    jump = NewJump (leave, enter, parent, continuation, ret);
    JumpContinuation (jump, next);
    RETURN (jump);
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
    StackObject	*stack;
    
    if (continuation_ref->value.tag != type_ref)
    {
	RaiseError ("setjump: not a reference %v", continuation_ref);
	RETURN (Zero);
    }
    stack = StackCopy (running->thread.stack);
    /*
     * Adjust stack for set jump return
     */
    STACK_DROP (stack, 2);
    continuation = NewContinuation (running->thread.frame, 
				    running->thread.pc + 1,
				    stack,
				    running->thread.catches,
				    running->thread.twixts);
    RefValueSet (continuation_ref, continuation);
#ifdef DEBUG_JUMP
    ContinuationTrace ("do_setjmp", continuation);
#endif
    RETURN (ret);
}

Value
do_longjmp (InstPtr *next, Value continuation, Value ret)
{
    ENTER ();
    TwixtPtr	leave, enter;

    if (!running)
	RETURN (Zero);
    if (continuation->value.tag != type_continuation)
    {
	RaiseError ("longjump: not a continuation %v", continuation);
	RETURN (Zero);
    }
#ifdef DEBUG_JUMP
    TraceContinuation ("do_longjmp from",
		       running->thread.frame,
		       running->thread.stack,
		       running->thread.catches,
		       running->thread.twixts,
		       running->thread.pc,
		       1);
    ContinuationTrace ("do_longjmp to", continuation);
#endif      
    /*
     * Check for intervening twixts
     */
    leave = running->thread.twixts;
    if (continuation)
	enter = continuation->continuation.twixts;
    else
	enter = 0;
    if (leave != enter)
	running->thread.jump = JumpBuild (leave,
					  enter,
					  continuation, ret,
					  next);
    else
	ContinuationJump (running, continuation, next);
    RETURN (ret);
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
MarkTwixt (void *object)
{
    TwixtPtr	twixt = object;

    MemReference (twixt->previous);
    MemReference (twixt->frame);
    MemReference (twixt->catches);
    MemReference (twixt->stack);
}

DataType    TwixtType = { MarkTwixt, 0 };

TwixtPtr
NewTwixt (TwixtPtr	previous,
	  FramePtr	frame, 
	  InstPtr	enter,
	  InstPtr	leave,
	  CatchPtr	catches,
	  StackObject	*stack)
{
    ENTER ();
    TwixtPtr	twixt;

    twixt = ALLOCATE (&TwixtType, sizeof (Twixt));
    twixt->previous = previous;
    if (previous)
	twixt->depth = previous->depth + 1;
    else
	twixt->depth = 1;
    twixt->frame = frame;
    twixt->enter = enter;
    twixt->leave = leave;
    twixt->catches = catches;
    twixt->stack = stack;
    RETURN (twixt);
}

/*
 * Set up for execution in a twixt context
 */
void
TwixtJump (Value thread, TwixtPtr twixt, Bool enter, InstPtr *next)
{
#ifdef DEBUG_JUMP
    TraceContinuation ("TwixtJump",
		       twixt->frame,
		       twixt->stack,
		       twixt->catches,
		       twixt->previous,
		       enter ? twixt->enter : twixt->leave,
		       1);
#endif
    thread->thread.frame = twixt->frame;
    thread->thread.stack = StackCopy (twixt->stack);
    thread->thread.catches = twixt->catches;
    thread->thread.twixts = twixt->previous;
    if (enter)
	*next = twixt->enter;
    else
	*next = twixt->leave;
}

int
TwixtDepth (TwixtPtr twixt)
{
    if (!twixt)
	return 0;
    return twixt->depth;
}

TwixtPtr
TwixtNext (TwixtPtr twixt, TwixtPtr last)
{
    if (last == twixt)
	return 0;
    while (last->previous != twixt)
	last = last->previous;
    return last;
}

void
RaiseException (Value thread, SymbolPtr except, BoxPtr args, InstPtr *next)
{
    ENTER ();
    Bool	caught = False;
    CatchPtr	catch;
    
    for (catch = thread->thread.catches; catch; catch = catch->previous)
	if (catch->exception == except)
	{
	    do_longjmp (next, catch->continuation, Zero);
	    if (args)
		ContinuationArgs (thread, args);
	    caught = True;
	    break;
	}
    if (!caught)
    {
	int	i;
	ExprPtr	stat = thread->thread.pc->base.stat;
	
	if (stat->base.file)
	    PrintError ("Unhandled exception \"%A\" at %A:%d\n", 
			except->symbol.name, stat->base.file, stat->base.line);
	else
	    PrintError ("Unhandled exception \"%A\"\n", 
			except->symbol.name);
	if (args)
	{
	    for (i = 0; i < args->nvalues; i++)
		PrintError ("\t%v\n", BoxValueGet (args, i));
	}
	SetSignalError ();
    }
    EXIT ();
}

SymbolPtr	    standardExceptions[_num_standard_exceptions];
StandardException   standardException;
BoxPtr		    standardExceptionArgs;
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
    BoxPtr	args;
    int		i;
    va_list	va;
    
    va_start (va, argc);
    args = NewBox (True, False, argc + 1);
    BoxValueSet (args, 0, NewStrString (string));
    if (argc)
    {
	for (i = 0; i < argc; i++)
	    BoxValueSet (args, i+1, va_arg (va, Value));
    }
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
