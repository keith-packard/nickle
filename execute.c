/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"nickle.h"

#define Stack(i) ((Value) STACK_ELT(thread->thread.stack, i))

/*
 * Instruction must be completed because changes have been
 * committed to storage.
 */

Bool	complete;

Bool	abortFinished;	    /* current thread is finished */
Bool	abortSuspend;	    /* current thread is suspending */

static ThreadState ThreadStep (Value thread);

static FramePtr
BuildFrame (Value thread, Value func, int nargs, InstPtr savePc)
{
    ENTER ();
    CodePtr	    code = func->func.code;
    int		    fe;
    FramePtr	    frame;
    Types	    *type;
    
    frame = NewFrame (func, thread->thread.frame, 
		      func->func.staticLink, 
		      code->func.dynamics,
		      func->func.statics);
    for (fe = 0; fe < nargs; fe++)
    {
	type = BoxTypesValue (code->func.dynamics, fe);
	if (!AssignTypeCompatiblep (type, Stack(fe)))
	{
	    RaiseError ("Incompatible type for argument %d function %t = %v",
			fe+1,
			type,
			Stack(fe));
	    RETURN (0);
	}
	if (!Stack(fe))
	    abort ();
	BoxValue (frame->frame, fe) = Copy (Stack(fe));
    }
    frame->function = func;
    frame->savePc = savePc;
    frame->saveCode = thread->thread.code;
    RETURN (frame);
}

static Value
ThreadCall (Value thread, InstPtr *next, int *stack)
{
    ENTER ();
    InstPtr	inst = thread->thread.pc;
    Value	value = thread->thread.v;
    CodePtr	code = value->func.code;
    FramePtr	frame;
    
    if (code->base.argc != -1 && code->base.argc != inst->ints.value)
    {
	RaiseError ("function requiring %d arguments was called with %d",
		code->base.argc, inst->ints.value);
	RETURN (value);
    }
    if (code->base.builtin)
    {
	Value	*values;
	int	arg;

	if (code->builtin.needsNext) 
	{
	    /*
	     * Let the non-local function handle the stack adjust
	     */
	    *stack = 0;
	    switch (code->base.argc) {
	    case -1:
		values = AllocateTemp (inst->ints.value * sizeof (Value));
		for (arg = 0; arg < inst->ints.value; arg++)
		    values[arg] = Stack(arg);
		value = (*code->builtin.b.builtinNJ)(next, inst->ints.value, values);
		break;
	    case 0:
		value = (*code->builtin.b.builtin0J)(next);
		break;
	    case 1:
		value = (*code->builtin.b.builtin1J)(next, Stack(0));
		break;
	    case 2:
		value = (*code->builtin.b.builtin2J)(next, Stack(0), Stack(1));
		break;
	    }
	}
	else 
	{
	    switch (code->base.argc) {
	    case -1:
		values = AllocateTemp (inst->ints.value * sizeof (Value));
		for (arg = 0; arg < inst->ints.value; arg++)
		    values[arg] = Stack(arg);
		value = (*code->builtin.b.builtinN)(inst->ints.value, values);
		break;
	    case 0:
		value = (*code->builtin.b.builtin0)();
		break;
	    case 1:
		value = (*code->builtin.b.builtin1)(Stack(0));
		break;
	    case 2:
		value = (*code->builtin.b.builtin2)(Stack(0), Stack(1));
		break;
	    case 3:
		value = (*code->builtin.b.builtin3)(Stack(0), Stack(1), Stack(2));
		break;
	    case 4:
		value = (*code->builtin.b.builtin4)(Stack(0), Stack(1), Stack(2),
						    Stack(3));
		break;
	    case 5:
		value = (*code->builtin.b.builtin5)(Stack(0), Stack(1), Stack(2),
						    Stack(3), Stack(4));
		break;
	    case 6:
		value = (*code->builtin.b.builtin6)(Stack(0), Stack(1), Stack(2),
						    Stack(3), Stack(4), Stack(5));
		break;
	    case 7:
		value = (*code->builtin.b.builtin7)(Stack(0), Stack(1), Stack(2),
						    Stack(3), Stack(4), Stack(5),
						    Stack(6));
		break;
	    default:
		value = Zero;
		break;
	    }
	}
    }
    else
    {
	frame = BuildFrame (thread, value, inst->ints.value, *next);
	if (exception)
	    RETURN (value);
	complete = True;
	thread->thread.frame = frame;
	thread->thread.code = code->func.obj;
	*next = ObjCode (thread->thread.code, 0);
    }
    RETURN (value);
}

/*
 * Call the pseudo function to initialize static values
 */
static void
ThreadStaticInit (Value thread, InstPtr *next)
{
    ENTER ();
    Value	value = thread->thread.v;
    CodePtr	code = value->func.code;
    FramePtr	frame;
    
    frame = NewFrame (value, thread->thread.frame,
		      value->func.staticLink,
		      0, value->func.statics);
    frame->function = value;
    frame->savePc = *next;
    frame->saveCode = thread->thread.code;
    complete = True;
    thread->thread.frame = frame;
    thread->thread.code = code->func.staticInit;
    *next = ObjCode (thread->thread.code, 0);
    EXIT ();
}

static void
ThreadAssign (Value ref, Value v)
{
    ENTER ();
    if (RefConstant(ref))
    {
	RaiseError ("Attempted assignment to constant box (%t) %v", 
		    RefType (ref), v);
    }
    else if (ref->ref.element >= ref->ref.box->nvalues)
    {
	RaiseError ("Attempted assignment beyond box bounds %v",
		    v);
    }
    else if (!AssignTypeCompatiblep (RefType (ref), v))
    {
	RaiseError ("Incompatible types in assignment %t = (%T) %v", 
		    RefType (ref), v->value.tag, v);
    }
    else
    {
	if (!v)
	    abort ();
	v = Copy (v);
	if (!exception)
	{
	    complete = True;
	    RefValue (ref) = v;
	}
    }
    EXIT ();
}

static Value
ThreadArray (Value thread, int ndim)
{
    ENTER ();
    int	    i;
    int	    *dims;

    dims = AllocateTemp (ndim * sizeof (int));
    for (i = 0; i < ndim; i++)
    {
	dims[i] = IntPart (Stack(i), "Invalid array dimension");
	if (exception)
	    RETURN (0);
    }
    RETURN (NewArray (False, typesPoly, ndim, dims));
}

static void
ThreadInitArray (Value thread, Value a, int ninit)
{
    ENTER ();
    int		i, j;

    if (ninit > a->array.ents)
    {
	RaiseError ("too many array initializers %d > %d",
		ninit, a->array.ents);
    }
    else
    {
	j = ninit;
	i = 0;
	while (j--)
	{
	    if (!AssignTypeCompatiblep (a->array.type,
					Stack(j)))
	    {
		RaiseError ("Incompatible type for array initializer %d %T = (%T) %v",
			i, a->array.type, Stack(j)->value.tag, Stack(j));
	    }
	    i++;
	}
	if (!exception)
	{
	    j = ninit;
	    i = 0;
	    while (j--)
	    {
		BoxValue (a->array.values, i) = Copy (Stack(j));
		i++;
	    }
	}
    }
    EXIT ();
}

static int
ThreadArrayIndex (Value thread, int ndim)
{
    ENTER ();
    int	    i;
    int	    dim;
    int	    part;
    Value   a;
    
    a = thread->thread.v;
    i = 0;
    for (dim = 0; dim < ndim; dim++)
    {
	part = IntPart (Stack(dim), "Invalid array index");
	if (exception)
	    break;
	if (part < 0 || a->array.dim[dim] <= part)
	{
	    RaiseError ("array index %d out of bounds (%d)", part,
		    a->array.dim[dim]);
	    break;
	}
	i = i * a->array.dim[dim] + part;
    }
    EXIT ();
    return i;
}

static void
ThreadCatch (Value thread, SymbolPtr exception, int offset)
{
    ENTER ();
    CatchPtr	catch;
    Value	continuation;

    continuation = NewContinuation (thread->thread.frame, 
				    thread->thread.pc + 1, 
				    StackCopy(thread->thread.stack),
				    thread->thread.catches,
				    thread->thread.twixts);
#ifdef DEBUG_JUMP
    ContinuationTrace ("ThreadCatch", continuation);
#endif
    catch = NewCatch (thread->thread.catches, continuation, exception);
    thread->thread.catches = catch;
    EXIT ();
}

static Bool
ThreadRaise (Value thread, int argc, SymbolPtr exception, InstPtr *next)
{
    ENTER ();
    CatchPtr	catch;
    Bool	ret = False;
    BoxPtr	args = 0;

#ifdef DEBUG_JUMP
    FilePuts (FileStdout, "    Raise: ");
    FilePuts (FileStdout, AtomName (exception->symbol.name));
    FilePuts (FileStdout, "\n");
#endif
    if (argc)
    {
	int i;
	args = NewBox (True, argc);
	for (i = 0; i < argc; i++)
	    BoxValue (args, i) = STACK_POP (thread->thread.stack);
    }
    for (catch = thread->thread.catches; catch; catch = catch->previous)
	if (catch->exception == exception)
	{
	    do_long_jump (next, catch->continuation, Zero);
	    if (args)
		ContinuationArgs (thread, args);
	    ret = True;
	    break;
	}
    EXIT ();
    return ret;
}

static void
ThreadTwixt (Value thread, int enterOffset, int leaveOffset)
{
    ENTER ();
    TwixtPtr	twixt, prev;

    prev = thread->thread.twixts;

    twixt = NewTwixt (prev,
		      thread->thread.frame,
		      thread->thread.pc + enterOffset,
		      thread->thread.pc + leaveOffset,
		      thread->thread.catches,
		      StackCopy (thread->thread.stack));
    thread->thread.twixts = twixt;
    EXIT ();
}

static inline ThreadState
ThreadStep (Value thread)
{
    ENTER ();
    InstPtr	inst, next;
    FramePtr	fp;
    int		i;
    int		stack = 0;
    Value	value, v, w;
    
    inst = thread->thread.pc;
    value = thread->thread.v;
    next = inst + 1;
    complete = False;
    switch (inst->base.opCode) {
    case OpNoop:
	break;
    case OpIf:
    case OpWhile:
    case OpFor:
    case OpQuest:
    case OpAnd:
	if (!True (value))
	    next = inst + inst->branch.offset;
	break;
    case OpElse:
    case OpEndWhile:
    case OpEndFor:
    case OpBreak:
    case OpContinue:
    case OpColon:
	if (!inst->branch.offset)
	{
	    RaiseError ("break/continue outside of loop");
	    break;
	}
	next = inst + inst->branch.offset;
	break;
    case OpDo:
    case OpOr:
	if (True (value))
	    next = inst + inst->branch.offset;
	break;
    case OpEnd:
	aborting = True;
	exception = True;
	abortFinished = True;
	break;
    case OpReturn:
	if (!thread->thread.frame)
	{
	    RaiseError ("return outside of function");
	    break;
	}
	if (exception)
	    break;
	complete = True;
	next = thread->thread.frame->savePc;
	thread->thread.code = thread->thread.frame->saveCode;
	thread->thread.frame = thread->thread.frame->previous;
	break;
    case OpName:
	switch (inst->var.name->symbol.class) {
	case class_global:
	    value = BoxValue (inst->var.name->global.value, 0);
	    break;
	case class_static:
	    for (i = 0, fp = thread->thread.frame; i < inst->var.staticLink; i++)
		fp = fp->staticLink;
	    value = BoxValue(fp->statics, inst->var.name->local.element);
	    break;
	case class_auto:
	case class_arg:
	    for (i = 0, fp = thread->thread.frame; i < inst->var.staticLink; i++)
		fp = fp->staticLink;
	    value = BoxValue(fp->frame, inst->var.name->local.element);
	    break;
	default:
	    break;
	}
	break;
    case OpNameRef:
	switch (inst->var.name->symbol.class) {
	case class_global:
	    value = NewRef (inst->var.name->global.value, 0);
	    break;
	case class_static:
	    for (i = 0, fp = thread->thread.frame; i < inst->var.staticLink; i++)
		fp = fp->staticLink;
	    value = NewRef(fp->statics, inst->var.name->local.element);
	    break;
	case class_auto:
	case class_arg:
	    for (i = 0, fp = thread->thread.frame; i < inst->var.staticLink; i++)
		fp = fp->staticLink;
	    value = NewRef (fp->frame, inst->var.name->local.element);
	    break;
	default:
	    break;
	}
	break;
    case OpConst:
	value = inst->constant.constant;
	break;
    case OpBuildArray:
	stack = inst->ints.value;
	value = ThreadArray (thread, stack);
	break;
    case OpInitArray:
	stack = inst->ints.value;
	ThreadInitArray (thread, value, stack);
	break;
    case OpBuildStruct:
        value = NewStruct (inst->structs.structs, False);
        break;
    case OpInitStruct:
	v = StructRef (value, inst->atom.atom);
	if (!v)
	{
	    RaiseError ("No member %A in %v", inst->atom.atom, value);
	    break;
	}
	w = Stack(stack); stack++;
	ThreadAssign (v, w);
	break;
    case OpArray:
    case OpArrayRef:
	if (value->value.tag != type_array)
	{
	    RaiseError ("attempt to use non-array as array");
	    break;
	}
	if (inst->ints.value != value->array.ndim)
	{
	    RaiseError ("mismatching number of dimensions %d != %d",
		    value->array.ndim, inst->ints.value);
	    break;
	}
	stack = inst->ints.value;
	i = ThreadArrayIndex (thread, stack);
	if (!exception)
	{
	    if (inst->base.opCode == OpArray)
		value = BoxValue (value->array.values, i);
	    else
		value = NewRef (value->array.values, i);
	}
	break;
    case OpCall:
	if (value->value.tag != type_func)
	{
	    RaiseError ("%v not a function", value);
	    break;
	}
        stack = inst->ints.value;
	value = ThreadCall (thread, &next, &stack);
	break;
    case OpArrow:
    case OpArrowRef:
	if (value->value.tag != type_ref ||
	    RefValue (value)->value.tag != type_struct)
	{
	    RaiseError ("%v not a structure reference", value);
	    break;
	}
	value = RefValue (value);
    case OpDot:
    case OpDotRef:
	if (value->value.tag != type_struct)
	{
	    RaiseError ("%v not a structure", value);
	    break;
	}
	if (inst->base.opCode == OpDot || inst->base.opCode == OpArrow)
	    v = StructValue (value, inst->atom.atom);
	else
	    v = StructRef (value, inst->atom.atom);
	if (!v)
	{
	    RaiseError ("No member %A in %v", inst->atom.atom, value);
	    break;
	}
	value = v;
	break;
    case OpObj:
	value = NewFunc (inst->code.code, thread->thread.frame);
	break;
    case OpStaticInit:
	/* Always follows OpObj so the function is sitting in value */
	ThreadStaticInit (thread, &next);
	break;
    case OpStaticDone:
	if (!thread->thread.frame)
	{
	    RaiseError ("StaticInitDone outside of function");
	    break;
	}
	if (exception)
	    break;
	complete = True;
	next = thread->thread.frame->savePc;
	thread->thread.code = thread->thread.frame->saveCode;
	thread->thread.frame = thread->thread.frame->previous;
	value = Stack (stack); stack++;
	break;
    case OpStar:
	if (value->value.tag != type_ref)
	{
	    RaiseError ("%v not a reference", value);
	    break;
	}
	value = RefValue (value);
	break;
    case OpUminus:
	value = Negate (value);
	break;
    case OpLnot:
	value = Lnot (value);
	break;
    case OpBang:
	value = Not (value);
	break;
    case OpFact:
	value = Factorial (value);
	break;
    case OpPreInc:
    case OpPostInc:
    case OpPreDec:
    case OpPostDec:
	if (value->value.tag != type_ref)
	{
	    RaiseError ("%v not an lvalue", value);
	    break;
	}
	v = RefValue (value);
	if (inst->base.opCode == OpPreInc || inst->base.opCode == OpPostInc)
	    w = Plus (v, One);
	else
	    w = Minus (v, One);
	ThreadAssign (value, w);
	value = v;
	if (inst->base.opCode == OpPreInc || inst->base.opCode == OpPreDec)
	    value = w;
	break;
    case OpPlus: 
	value = Plus (Stack(stack), value); stack++;
	break;
    case OpMinus: 
	value = Minus (Stack(stack), value); stack++;
	break;
    case OpTimes: 
	value = Times (Stack(stack), value); stack++;
	break;
    case OpDivide: 
	value = Divide (Stack(stack), value); stack++;
	break;
    case OpDiv: 
	value = Div (Stack(stack), value); stack++;
	break;
    case OpMod: 
	value = Mod (Stack(stack), value); stack++;
	break;
    case OpPow: 
	value = Pow (Stack(stack), value); stack++;
	break;
    case OpShiftL:
	value = ShiftL (Stack(stack), value); stack++;
	break;
    case OpShiftR:
	value = ShiftR (Stack(stack), value); stack++;
	break;
    case OpLxor: 
	value = Lxor (Stack(stack), value); stack++;
	break;
    case OpLand: 
	value = Land (Stack(stack), value); stack++;
	break;
    case OpLor: 
	value = Lor (Stack(stack), value); stack++;
	break;
    case OpFunction:
    case OpAssign:
    case OpAssignPlus: 
    case OpAssignMinus:
    case OpAssignTimes:
    case OpAssignDivide:
    case OpAssignDiv:
    case OpAssignMod:
    case OpAssignPow:
    case OpAssignShiftL:
    case OpAssignShiftR:
    case OpAssignLxor:
    case OpAssignLand:
    case OpAssignLor:
	if (value->value.tag != type_ref)
	    break;
	v = Stack(stack); stack++;
	switch (inst->base.opCode) {
	case OpFunction:
	case OpAssign:
	    break;
	case OpAssignPlus: 
	    v = Plus (RefValue (value), v); break;
	case OpAssignMinus:
	    v = Minus (RefValue (value), v); break;
	case OpAssignTimes:
	    v = Times (RefValue (value), v); break;
	case OpAssignDivide:
	    v = Divide (RefValue (value), v); break;
	case OpAssignDiv:
	    v = Div (RefValue (value), v); break;
	case OpAssignMod:
	    v = Mod (RefValue (value), v); break;
	case OpAssignShiftL:
	    v = ShiftL (RefValue (value), v); break;
	case OpAssignShiftR:
	    v = ShiftR (RefValue (value), v); break;
	case OpAssignPow:
	    v = Pow (RefValue (value), v); break;
	case OpAssignLxor:
	    v = Lxor (RefValue (value), v); break;
	case OpAssignLand:
	    v = Land (RefValue (value), v); break;
	case OpAssignLor:
	    v = Lor (RefValue (value), v); break;
	default:
	    break;
	}
	ThreadAssign (value, v);
	value = v;
	break;
    case OpEq:
	value = Equal (Stack(stack), value); stack++; break;
    case OpNe:
	value = NotEqual (Stack(stack), value); stack++; break;
    case OpLt:
	value = Less (Stack(stack), value); stack++; break;
    case OpGt:
	value = Greater (Stack(stack), value); stack++; break;
    case OpLe:
	value = LessEqual (Stack(stack), value); stack++; break;
    case OpGe:
	value = GreaterEqual (Stack(stack), value); stack++; break;
    case OpFork:
	value = NewThread (thread->thread.frame, inst->obj.obj); break;
    case OpCatch:
	ThreadCatch (thread, inst->catch.exception, inst->catch.offset);
	next = inst + inst->catch.offset;
	break;
    case OpException:
	next = inst + inst->branch.offset;
	break;
    case OpEndCatch:
	thread->thread.catches = thread->thread.catches->previous;
	break;
    case OpRaise:
	if (!ThreadRaise (thread, inst->raise.argc, inst->raise.exception, 
			  &next))
	    RaiseError ("Unhandled exception \"%A\"",
			inst->raise.exception->symbol.name);
	break;
    case OpTwixt:
	ThreadTwixt (thread, inst->twixt.enter, inst->twixt.leave);
	break;
    case OpTwixtDone:
	thread->thread.twixts = thread->thread.twixts->previous;
	break;
    case OpEnterDone:
	if (!True (value))
	{
	    next = inst + inst->branch.offset;
	    thread->thread.jump = 0;
	}
	else
	{
	    if (thread->thread.jump)
		value = JumpContinuation (thread->thread.jump, &next);
	}
	break;
    case OpLeaveDone:
	if (thread->thread.jump)
	    value = JumpContinuation (thread->thread.jump, &next);
	else
	    next = inst + inst->branch.offset;
	break;
    }
    if (!exception || complete)
    {
	if (!value)
	    abort ();
	/* this instruction has been completely executed */
	thread->thread.partial = 0;
	thread->thread.v = value;
	if (stack)
	    STACK_DROP (thread->thread.stack, stack);
	if (inst->base.push)
	    STACK_PUSH (thread->thread.stack, value);
	thread->thread.pc = next;
	if (thread == running && thread->thread.priority > PriorityMin)
	    ThreadStepped (thread);
    }
    else
    {
	if (abortSuspend)
	{
	    abortSuspend = False;
	    ThreadSetState (thread, ThreadSuspended);
	}
	if (abortFinished)
	{
	    abortFinished = False;
	    ThreadFinish (thread);
	}
	if (abortException)
	{
	    abortException = False;
	    RaiseError ("Floating point exception");
	}
	if (abortError)
	{
	    abortError = False;
	    DebugSetFrame (NewContinuation (thread->thread.frame,
					    inst,
					    thread->thread.stack,
					    thread->thread.catches,
					    thread->thread.twixts),
			   0);
	    ThreadFinish (thread);
	}
    }
    EXIT ();
    return thread->thread.state;
}

#include	<signal.h>

void
ThreadsRun (Value thread, Value lex)
{
    abortInterrupt = False;
    for (;;)
    {
	if (aborting)
	{
	    aborting = False;
	    exception = False;
	    if (abortInterrupt)
	    {
		abortInterrupt = False;
		if (thread)
		    ThreadSetState (thread, ThreadInterrupted);
		(void) write (2, "<abort>\n", 8);
	    }
	    if (abortTimer)
	    {
		abortTimer = False;
		TimerInterrupt ();
	    }
	    if (abortIo)
	    {
		abortIo = False;
		IoInterrupt ();
	    }
	    if (lex && !(lex->file.flags & FileInputBlocked))
		break;
	}
	else if (thread && !Runnable (thread))
	    break;
	else if (running)
	    ThreadStep (running);
	else
	{
	    sigset_t	set, oset;

	    sigfillset (&set);
	    sigprocmask (SIG_SETMASK, &set, &oset);
	    if (!aborting)
		sigsuspend (&oset);
	    sigprocmask (SIG_SETMASK, &oset, &set);
	}
    }
}
