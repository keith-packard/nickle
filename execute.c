/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

#define Stack(i) ((Value) STACK_ELT(thread->thread.continuation.stack, i))

/*
 * Instruction must be completed because changes have been
 * committed to storage.
 */

Bool	complete;

Bool	signalFinished;	    /* current thread is finished */
Bool	signalSuspend;	    /* current thread is suspending */

#define Arg(n)  Stack((argc - 1) - (n))

static FramePtr
BuildFrame (Value thread, Value func, Bool staticInit, Bool tail,
	    Bool varargs, int nformal,
	    int argc, InstPtr savePc, ObjPtr saveObj)
{
    ENTER ();
    CodePtr	    code = func->func.code;
    FuncBodyPtr	    body = staticInit ? &code->func.staticInit : &code->func.body;
    int		    fe;
    FramePtr	    frame;
    
    frame = thread->thread.continuation.frame;
    if (tail)
	frame = frame->previous;
    frame = NewFrame (func, frame,
		      func->func.staticLink, 
		      body->dynamics,
		      func->func.statics);
    for (fe = 0; fe < nformal; fe++)
	BoxValueSet (frame->frame, fe, Copy (Arg(fe)));
    if (varargs)
    {
	int	extra = argc - nformal;
	Value	array;
	
	array = NewArray (True, typesPoly, 1, &extra);
	BoxValueSet (frame->frame, fe, array);
	for (; fe < argc; fe++)
	    BoxValueSet (array->array.values, fe-nformal, Copy (Arg(fe)));
    }
    if (tail)
    {
	frame->savePc = thread->thread.continuation.frame->savePc;
	frame->saveObj = thread->thread.continuation.frame->saveObj;
    }
    else
    {
	frame->savePc = savePc;
	frame->saveObj = saveObj;
    }
    RETURN (frame);
}

static Value
ThreadCall (Value thread, Bool tail, InstPtr *next, int *stack)
{
    ENTER ();
    Value	value = thread->thread.continuation.value;
    CodePtr	code = value->func.code;
    int		argc = *stack;
    FramePtr	frame;
    ArgType	*argt;
    int		fe;
    
    argt = code->base.args;
    fe = 0;
    while (fe < argc || (argt && !argt->varargs))
    {
	if (!argt)
	{
	    RaiseStandardException (exception_invalid_argument, 
				    "Too many parameters",
				    2, NewInt (argc), NewInt(code->base.argc));
	    RETURN (value);
	}
	if (fe == argc)
	{
	    RaiseStandardException (exception_invalid_argument, 
				    "Too few arguments",
				    2, NewInt (argc), NewInt(code->base.argc));
	    RETURN (value);
	}
	if (!TypeCompatibleAssign (argt->type, Arg(fe)))
	{
	    RaiseStandardException (exception_invalid_argument, 
				    "Incompatible argument",
				    2, NewInt (fe), Arg(fe));
	    RETURN (value);
	}
	fe++;
	if (!argt->varargs)
	    argt = argt->next;
    }

    if (code->base.builtin)
    {
	Value	*values;
	int	arg;
	int	formal;

	formal = code->base.argc;
	if (code->base.varargs)
	    formal = -1;

	if (code->builtin.needsNext) 
	{
	    /*
	     * Let the non-local function handle the stack adjust
	     */
	    *stack = 0;
	    switch (formal) {
	    case -1:
		values = AllocateTemp (argc * sizeof (Value));
		for (arg = 0; arg < argc; arg++)
		    values[arg] = Arg(arg);
		value = (*code->builtin.b.builtinNJ)(next, argc, values);
		break;
	    case 0:
		value = (*code->builtin.b.builtin0J)(next);
		break;
	    case 1:
		value = (*code->builtin.b.builtin1J)(next, Arg(0));
		break;
	    case 2:
		value = (*code->builtin.b.builtin2J)(next, Arg(0), Arg(1));
		break;
	    }
	}
	else 
	{
	    switch (formal) {
	    case -1:
		values = AllocateTemp (argc * sizeof (Value));
		for (arg = 0; arg < argc; arg++)
		    values[arg] = Arg(arg);
		value = (*code->builtin.b.builtinN)(argc, values);
		break;
	    case 0:
		value = (*code->builtin.b.builtin0)();
		break;
	    case 1:
		value = (*code->builtin.b.builtin1)(Arg(0));
		break;
	    case 2:
		value = (*code->builtin.b.builtin2)(Arg(0), Arg(1));
		break;
	    case 3:
		value = (*code->builtin.b.builtin3)(Arg(0), Arg(1), Arg(2));
		break;
	    case 4:
		value = (*code->builtin.b.builtin4)(Arg(0), Arg(1), Arg(2),
						    Arg(3));
		break;
	    case 5:
		value = (*code->builtin.b.builtin5)(Arg(0), Arg(1), Arg(2),
						    Arg(3), Arg(4));
		break;
	    case 6:
		value = (*code->builtin.b.builtin6)(Arg(0), Arg(1), Arg(2),
						    Arg(3), Arg(4), Arg(5));
		break;
	    case 7:
		value = (*code->builtin.b.builtin7)(Arg(0), Arg(1), Arg(2),
						    Arg(3), Arg(4), Arg(5),
						    Arg(6));
		break;
	    default:
		value = Zero;
		break;
	    }
	}
#undef Arg
	if (tail && !aborting)
	{
	    complete = True;
	    *next = thread->thread.continuation.frame->savePc;
	    thread->thread.continuation.frame = thread->thread.continuation.frame->previous;
	}
    }
    else
    {
	frame = BuildFrame (thread, value, False, tail, code->base.varargs,
			    code->base.argc, argc, *next, thread->thread.continuation.obj);
	if (aborting)
	    RETURN (value);
	complete = True;
	thread->thread.continuation.frame = frame;
	thread->thread.continuation.obj = code->func.body.obj;
	*next = ObjCode (code->func.body.obj, 0);
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
    Value	value = thread->thread.continuation.value;
    CodePtr	code = value->func.code;
    FramePtr	frame;
    
    frame = BuildFrame (thread, value, True, False, False, 0, 0, 
			*next, thread->thread.continuation.obj);
    if (aborting)
	return;
    complete = True;
    thread->thread.continuation.frame = frame;
    thread->thread.continuation.obj = code->func.staticInit.obj;
    *next = ObjCode (code->func.staticInit.obj, 0);
    EXIT ();
}

static INLINE void
ThreadAssign (Value ref, Value v, Bool initialize)
{
    ENTER ();
    if (!ValueIsRef (ref))
    {
	RaiseStandardException (exception_invalid_binop_values,
				"Attempted store through non reference",
				2, ref, v);
    }
    else if (RefConstant(ref) && !initialize)
    {
	RaiseStandardException (exception_readonly_box,
				"Attempted assignment to constant box",
				1, v);
    }
    else if (ref->ref.element >= ref->ref.box->nvalues)
    {
	RaiseStandardException (exception_invalid_array_bounds,
				"Attempted assignment beyond box bounds",
				2, NewInt(ref->ref.element), v);
    }
    else if (!TypeCompatibleAssign (RefType (ref), v))
    {
	RaiseStandardException (exception_invalid_argument,
				"Incompatible types in assignment",
				2, NewInt(ref->ref.element), v);
    }
    else
    {
	if (!v)
	    abort ();
	v = Copy (v);
	if (!aborting)
	{
	    complete = True;
	    RefValueSet (ref, v);
	}
    }
    EXIT ();
}

static Value
ThreadArray (Value thread, int ndim, Types *type)
{
    ENTER ();
    int	    i;
    int	    *dims;

    dims = AllocateTemp (ndim * sizeof (int));
    for (i = 0; i < ndim; i++)
    {
	dims[i] = IntPart (Stack(i), "Invalid array dimension");
	if (aborting)
	    RETURN (0);
    }
    RETURN (NewArray (False, type, ndim, dims));
}

static int
ThreadInitArrayPart (Value thread, Value a, int base, int dim, int s)
{
    ENTER();
    int	    ninit;
    int	    n;
    int	    ns;
    int	    size;
    Bool    replicate = False;

    ninit = IntPart (Stack (s), "Invalid array initialization");
    if (ninit < 0)
    {
	replicate = True;
	ninit = -ninit;
    }
    ++s;
    if (aborting)
    {
	EXIT ();
	return s;
    }
    if (ninit > a->array.dim[dim])
    {
	RaiseStandardException (exception_invalid_argument,
				"too many array initializers",
				2, NewInt (ninit), NewInt (a->array.dim[dim]));
	EXIT ();
	return s;
    }
    if (dim == a->array.ndim - 1)
    {
	if (replicate)
	    n = a->array.dim[dim] - 1;
	else
	    n = ninit - 1;
	for (; n >= 0; n--)
	{
	    if (!TypeCompatibleAssign (a->array.type, Stack(s)))
	    {
		RaiseStandardException (exception_invalid_argument,
				"Incompatible types in array initialization",
				2, NewInt(n), Stack(s));
	    }
	    BoxValueSet (a->array.values, base + n, Copy (Stack(s)));
	    if (n < ninit)
		s++;
	}
    }
    else
    {
	size = 1;
	for (n = dim + 1; n < a->array.ndim; n++)
	    size = size * a->array.dim[n];
	if (replicate)
	    n = a->array.dim[dim] - 1;
	else
	    n = ninit - 1;
	base += size * (n);
	for (; n >= 0; n--)
	{
	    ns = ThreadInitArrayPart (thread, a, base, dim+1, s);
	    base -= size;
	    if (n < ninit)
		s = ns;
	}
    }
    EXIT ();
    return s;
}

static void
ThreadInitArray (Value thread, Value a, int ninit)
{
    ENTER ();
    ThreadInitArrayPart (thread, a, 0, 0, 0);
    EXIT ();
}

static int
ThreadArrayIndex (Value thread, int ndim)
{
    int	    i;
    int	    dim;
    int	    part;
    Value   a;
    Value   d;
    
    a = thread->thread.continuation.value;
    d = Stack(0);
    if (!ValueIsInt(d) || 
	(i = d->ints.value) < 0 || 
	a->array.dim[0] <= i)
    {
	RaiseStandardException (exception_invalid_array_bounds,
				"Array index out of bounds",
				2, a, Stack(0));
	return 0;
    }
    for (dim = 1; dim < ndim; dim++)
    {
	d = Stack(dim);
	if (!ValueIsInt(d) || 
	    (part = d->ints.value) < 0 || 
	    a->array.dim[dim] <= part)
	{
	    RaiseStandardException (exception_invalid_array_bounds,
				    "Array index out of bounds",
				    2, a, Stack(dim));
	    return 0;
	}
	i = i * a->array.dim[dim] + part;
    }
    return i;
}

static Value
ThreadRaise (Value thread, int argc, SymbolPtr exception, InstPtr *next)
{
    ENTER ();
    Value   args;
    int i;

#ifdef DEBUG_JUMP
    FilePrintf (FileStdout, "    Raise: %A\n", exception->symbol.name);
#endif
    /*
     * Build and array to hold the arguments, this will end up
     * in the thread's value on entry to the catch block
     */
    args = NewArray (False, typesPoly, 1, &argc);
    for (i = 0; i < argc; i++)
        BoxValueSet (args->array.values, i, STACK_POP (thread->thread.continuation.stack));
    RaiseException (thread, exception, args, next);
    RETURN (args);
}

static Value
ThreadExceptionCall (Value thread, InstPtr *next, int *stack)
{
    ENTER ();
    Value   args;
    Value   ret;
    int	    argc;
    int	    i;

    /*
     * The compiler places a Noop with the push flag set
     * before the function is stuffed in the thread value,
     * this pushes the array of arguments carefully crafted
     * in ThreadRaise above.  Fetch the argument array and
     * push all of them onto the stack.
     */
    args = Stack(0);
    if (!ValueIsArray (args))
    {
	RaiseStandardException (exception_invalid_argument,
				"exception call argument must be array",
				1, args);
	*stack = 1;
	RETURN (Zero);
    }
    if (aborting)
    {
	*stack = 1;
	RETURN (Zero);
    }
    complete = True;
    argc = args->array.ents;
    i = argc;
    while (--i >= 0)
	STACK_PUSH (thread->thread.continuation.stack, BoxValue(args->array.values, i));
    /*
     * Call the function
     */
    ret = ThreadCall (thread, False, next, &argc);
    /*
     * Account for the argument array
     */
    *stack = 1 + argc;
    RETURN (ret);
}

static void
ThreadUnwind (Value thread, Value ret, int twixts, int catches, InstPtr *next)
{
    ENTER ();
    Value	continuation;
    CatchPtr	catch;
    TwixtPtr	twixt;
    
    /*
     * Create a continuation that unwinds the twixts
     * and catches and then ends up executing the next
     * instruction
     */
    continuation = NewContinuation (&thread->thread.continuation, *next);
    /*
     * Unwind correct number of twixts
     */
    twixt = continuation->continuation.twixts;
    while (twixts--)
	twixt = twixt->continuation.twixts;
    
    continuation->continuation.twixts = twixt;
    /*
     * Unwind correct number of catches
     */
    catch = continuation->continuation.catches;
    while (catches--)
	catch = catch->continuation.catches;;
    continuation->continuation.catches = catch;
    /*
     * And jump
     */
    if (!aborting)
    {
	complete = True;
	ContinuationJump (thread, &continuation->continuation, ret, next);
    }
    EXIT ();
}

#define ThreadBoxCheck(box,i,type) (BoxValueGet(box,i) == 0 ? ThreadBoxSetDefault(box,i,type) : 0)

static void
ThreadBoxSetDefault (BoxPtr box, int i, Types *type)
{
    if (BoxValueGet (box, i) == 0)
    {
	Types   *ctype = TypesCanon (type);
	StructType  *st = ctype->structs.structs;
	
	switch (ctype->base.tag) {
	case types_union:
	    BoxValueSet (box, i, NewUnion (st, False));
	    break;
	case types_struct:
	    BoxValueSet (box, i, NewStruct (st, False));
	    box = BoxValueGet (box, i)->structs.values;
	    for (i = 0; i < st->nelements; i++)
		ThreadBoxCheck (box, i, StructTypeElements(st)[i].type);
	    break;
	default:
	    break;
	}
    }
}

#include	<signal.h>

void
ThreadsRun (Value thread, Value lex)
{
    signalInterrupt = False;
    for (;;)
    {
	if (signaling)
	{
	    signaling = False;
	    aborting = False;
	    /*
	     * Check for pending external signals
	     */
	    if (signalInterrupt)
	    {
		signalInterrupt = False;
		if (thread)
		    ThreadSetState (thread, ThreadInterrupted);
		(void) write (2, "<abort>\n", 8);
	    }
	    if (signalTimer)
	    {
		signalTimer = False;
		TimerInterrupt ();
	    }
	    if (signalIo)
	    {
		signalIo = False;
		IoInterrupt ();
	    }
	    if (signalProfile)
	    {
		signalProfile = False;
		ProfileInterrupt (running);
	    }
	    if (lex && !(lex->file.flags & (FileInputBlocked|FileOutputBlocked)))
		break;
	}
	else if (thread && !Runnable (thread))
	    break;
	else if (running)
	{
	    ENTER ();
	    Value   thread = running;
	    InstPtr	inst, next;
	    FramePtr	fp;
	    int		i;
	    int		stack = 0;
	    Value	value, v, w;
	    BoxPtr	box;

	    inst = thread->thread.continuation.pc;
	    value = thread->thread.continuation.value;
	    next = inst + 1;
	    complete = False;
	    /*    InstDump (inst, 1, 0, 0, 0); */
	    switch (inst->base.opCode) {
	    case OpNoop:
		break;
	    case OpBranch:
		next = inst + inst->branch.offset;
		break;
	    case OpBranchFalse:
		if (!True (value))
		    next = inst + inst->branch.offset;
		break;
	    case OpBranchTrue:
		if (True (value))
		    next = inst + inst->branch.offset;
		break;
	    case OpCase:
		value = Equal (Stack(stack), value);
		if (True (value))
		{
		    next = inst + inst->branch.offset;
		    stack++;
		}
		break;
	    case OpDefault:
		next = inst + inst->branch.offset;
		stack++;
		break;
	    case OpTagCase:
		if (!ValueIsUnion(value))
		{
		    RaiseStandardException (exception_invalid_argument,
					    "union switch expression not union",
					    2,
					    Zero, value);
		    break;
		}
		if (value->unions.tag == inst->tagcase.tag)
		    next = inst + inst->tagcase.offset;
		break;
	    case OpReturnVoid:
		value = Void;
		/* fall through */
	    case OpReturn:
		if (!thread->thread.continuation.frame)
		{
		    RaiseError ("return outside of function");
		    break;
		}
		if (!TypeCompatibleAssign (thread->thread.continuation.frame->function->func.code->base.type,
					   value))
		{
		    RaiseStandardException (exception_invalid_argument,
					    "Incompatible type in return",
					    2, NewInt (0), value);
		    break;
		}
		if (aborting)
		    break;
		complete = True;
		next = thread->thread.continuation.frame->savePc;
		thread->thread.continuation.obj = thread->thread.continuation.frame->saveObj;
		thread->thread.continuation.frame = thread->thread.continuation.frame->previous;
		break;
	    case OpGlobal:
	    case OpGlobalRef:
	    case OpGlobalRefStore:
		box = inst->var.name->global.value;
		if (inst->base.opCode != OpGlobalRefStore)
		    ThreadBoxCheck (box, 0, inst->var.name->symbol.type);
		if (inst->base.opCode == OpGlobal)
		    value = BoxValue (box, 0);
		else
		    value = NewRef (box, 0);
		break;
	    case OpStatic:
	    case OpStaticRef:
	    case OpStaticRefStore:
		for (i = 0, fp = thread->thread.continuation.frame; i < inst->var.staticLink; i++)
		    fp = fp->staticLink;
		box = fp->statics;
		i = inst->var.name->local.element;
		if (inst->base.opCode != OpStaticRefStore)
		    ThreadBoxCheck (box, i, inst->var.name->symbol.type);
		if (inst->base.opCode == OpStatic)
		    value = BoxValue (box, i);
		else
		    value = NewRef (box, i);
		break;
	    case OpLocal:
	    case OpLocalRef:
	    case OpLocalRefStore:
		for (i = 0, fp = thread->thread.continuation.frame; i < inst->var.staticLink; i++)
		    fp = fp->staticLink;
		box = fp->frame;
		i = inst->var.name->local.element;
		if (inst->base.opCode != OpLocalRefStore)
		    ThreadBoxCheck (box, i, inst->var.name->symbol.type);
		if (inst->base.opCode == OpLocal)
		    value = BoxValue (box, i);
		else
		    value = NewRef (box, i);
		break;
	    case OpConst:
		value = inst->constant.constant;
		break;
	    case OpBuildArray:
		stack = inst->array.ndim;
		value = ThreadArray (thread, stack, inst->array.type);
		break;
	    case OpInitArray:
		stack = inst->ints.value;
		ThreadInitArray (thread, value, stack);
		break;
	    case OpBuildStruct:
		value = NewStruct (inst->structs.structs, False);
		break;
	    case OpInitStruct:
		w = Stack(stack); stack++;
		v = StructRef (w, inst->atom.atom);
		if (!v)
		{
		    RaiseStandardException (exception_invalid_struct_member,
					    "Invalid struct member",
					    2, v, 
					    NewStrString (AtomName (inst->atom.atom)));
		    break;
		}
		ThreadAssign (v, value, True);
		value = w;
		break;
	    case OpBuildUnion:
		value = NewUnion (inst->structs.structs, False);
		break;
	    case OpInitUnion:
		v = UnionRef (value, inst->atom.atom);
		if (!v)
		{
		    RaiseStandardException (exception_invalid_struct_member,
					    "Invalid union member",
					    2, value, 
					    NewStrString (AtomName (inst->atom.atom)));
		    break;
		}
		w = Stack(stack); stack++;
		ThreadAssign (v, w, True);
		break;
	    case OpArray:
	    case OpArrayRef:
	    case OpArrayRefStore:
		switch (ValueTag(value)) {
		    char    *s;
		case type_string:
		    if (inst->base.opCode != OpArray)
		    {
			RaiseStandardException (exception_invalid_unop_value,
						"Strings aren't addressable",
						1, value);
			break;
		    }
		    if (inst->ints.value != 1)
		    {
			RaiseStandardException (exception_invalid_binop_values,
						"Strings have only 1 dimension",
						2, NewInt (inst->ints.value), value);
			break;
		    }
		    stack = 0;
		    v = Stack(stack); stack++;
		    i = IntPart (v, "Invalid string index");
		    if (aborting)
			break;
		    s = StringChars (&value->string);
		    if (i < 0 || strlen (s) < i)
		    {
			RaiseStandardException (exception_invalid_binop_values,
						"String index out of bounds",
						2, v, value);
			break;
		    }
		    value = NewInt (s[i]);
		    break;
		case type_array:
		    if (inst->ints.value != value->array.ndim)
		    {
			RaiseStandardException (exception_invalid_binop_values,
						"Mismatching dimensionality",
						2, NewInt (inst->ints.value), value);
			break;
		    }
		    stack = inst->ints.value;
		    i = ThreadArrayIndex (thread, stack);
		    if (!aborting)
		    {
			if (inst->base.opCode != OpArrayRefStore)
			    ThreadBoxCheck (value->array.values, i, value->array.type);
			if (inst->base.opCode == OpArray)
			    value = BoxValue (value->array.values, i);
			else
			    value = NewRef (value->array.values, i);
		    }
		    break;
		default:
		    RaiseStandardException (exception_invalid_unop_value,
					    "Not an array",
					    1, value);
		    break;
		}
		break;
	    case OpCall:
	    case OpTailCall:
		if (!ValueIsFunc(value))
		{
		    RaiseStandardException (exception_invalid_unop_value,
					    "Not a function",
					    1, value);
		    break;
		}
		stack = inst->ints.value;
		value = ThreadCall (thread, inst->base.opCode == OpTailCall,
				    &next, &stack);
		break;
	    case OpArrow:
	    case OpArrowRef:
	    case OpArrowRefStore:
		if (!ValueIsRef(value) ||
		    (!ValueIsStruct(RefValue (value)) && !ValueIsUnion(RefValue (value))))
		{
		    RaiseStandardException (exception_invalid_unop_value,
					    "Not a struct/union reference",
					    1, value);
		    break;
		}
		value = RefValue (value);
	    case OpDot:
	    case OpDotRef:
	    case OpDotRefStore:
		switch (ValueTag(value)) {
		default:
		    RaiseStandardException (exception_invalid_unop_value,
					    "Not a struct/union",
					    1, value);
		    break;
		case type_struct:
		    if (inst->base.opCode == OpDot || inst->base.opCode == OpArrow)
			v = StructValue (value, inst->atom.atom);
		    else
			v = StructRef (value, inst->atom.atom);
		    if (!v)
		    {
			RaiseStandardException (exception_invalid_struct_member,
						"no such struct member",
						2, value, 
						NewStrString (AtomName (inst->atom.atom)));
			break;
		    }
		    value = v;
		    break;
		case type_union:
		    if (inst->base.opCode == OpDot || inst->base.opCode == OpArrow)
			v = UnionValue (value, inst->atom.atom);
		    else
		    {
			v = UnionRef (value, inst->atom.atom);
		    }
		    if (!v)
		    {
			if (StructTypes (value->unions.type, inst->atom.atom))
			    RaiseStandardException (exception_invalid_struct_member,
						    "requested union tag not current",
						    2, value,
						    NewStrString (AtomName (inst->atom.atom)));
			else
			    RaiseStandardException (exception_invalid_struct_member,
						    "no such union tag",
						    2, value, 
						    NewStrString (AtomName (inst->atom.atom)));
			break;
		    }
		    value = v;
		    break;
		}
		break;
	    case OpObj:
		value = NewFunc (inst->code.code, thread->thread.continuation.frame);
		break;
	    case OpStaticInit:
		/* Always follows OpObj so the function is sitting in value */
		ThreadStaticInit (thread, &next);
		break;
	    case OpStaticDone:
		if (!thread->thread.continuation.frame)
		{
		    RaiseError ("StaticInitDone outside of function");
		    break;
		}
		if (aborting)
		    break;
		complete = True;
		next = thread->thread.continuation.frame->savePc;
		thread->thread.continuation.obj = thread->thread.continuation.frame->saveObj;
		thread->thread.continuation.frame = thread->thread.continuation.frame->previous;
		/* Fetch the Obj from the stack */
		value = Stack (stack); stack++;
		break;
	    case OpBinOp:
		value = BinaryOperate (Stack(stack), value, inst->binop.op); stack++;
		break;
	    case OpBinFunc:
		value = (*inst->binfunc.func) (Stack(stack), value); stack++;
		break;
	    case OpUnOp:
		value = UnaryOperate (value, inst->unop.op);
		break;
	    case OpUnFunc:
		value = (*inst->unfunc.func) (value);
		break;
	    case OpPreOp:
		v = BinaryOperate (Dereference (value), One, inst->binop.op);
		ThreadAssign (value, v, False);
		value = v;
		break;
	    case OpPostOp:
		v = Dereference (value);
		ThreadAssign (value, BinaryOperate (v, One, inst->binop.op), False);
		value = v;
		break;
	    case OpAssign:
		v = Stack(stack); stack++;
		ThreadAssign (value, v, inst->assign.initialize);
		value = v;
		break;
	    case OpAssignOp:
		v = BinaryOperate (Dereference (value), Stack(stack), inst->binop.op);
		stack++;
		ThreadAssign (value, v, False);
		value = v;
		break;
	    case OpAssignFunc:
		v = (*inst->binfunc.func) (Dereference (value), Stack(stack));
		stack++;
		ThreadAssign (value, v, False);
		value = v;
		break;
	    case OpFork:
		value = NewThread (thread->thread.continuation.frame, inst->obj.obj); 
		break;
	    case OpCatch:
		if (aborting)
		    break;
		thread->thread.continuation.catches = NewCatch (thread,
							   inst->catch.exception);
		complete = True;
		next = inst + inst->catch.offset;
		break;
	    case OpEndCatch:
		if (aborting)
		    break;
		thread->thread.continuation.catches = thread->thread.continuation.catches->continuation.catches;
		complete = True;
		break;
	    case OpRaise:
		if (aborting)
		    break;
		complete = True;
		value = ThreadRaise (thread, inst->raise.argc, inst->raise.exception, &next);
		break;
	    case OpExceptionCall:
		ThreadExceptionCall (thread, &next, &stack);
		break;
	    case OpTwixt:
		if (aborting)
		    break;
		thread->thread.continuation.twixts = NewTwixt (&thread->thread.continuation,
							  thread->thread.continuation.pc + inst->twixt.enter,
							  thread->thread.continuation.pc + inst->twixt.leave);
		complete = True;
		break;
	    case OpTwixtDone:
		if (aborting)
		    break;
		thread->thread.continuation.twixts = thread->thread.continuation.twixts->continuation.twixts;
		complete = True;
		break;
	    case OpEnterDone:
		if (!True (value))
		{
		    if (aborting)
			break;
		    next = inst + inst->branch.offset;
		    thread->thread.jump = 0;
		    complete = True;
		}
		else if (thread->thread.jump)
		{
		    if (aborting)
			break;
		    value = JumpContinue (thread, &next);
		    complete = True;
		}
		break;
	    case OpLeaveDone:
		if (thread->thread.jump)
		{
		    if (aborting)
			break;
		    value = JumpContinue (thread, &next);
		    complete = True;
		}
		else
		    next = inst + inst->branch.offset;
		break;
	    case OpUnwind:
		ThreadUnwind (thread, value, inst->unwind.twixt, inst->unwind.catch, &next);
		break;
	    case OpEnd:
		SetSignalFinished ();
		break;
	    default:
		break;
	    }
	    if (!aborting || complete)
	    {
		/* this instruction has been completely executed */
		thread->thread.partial = 0;
		thread->thread.continuation.value = value;
		if (stack)
		    STACK_DROP (thread->thread.continuation.stack, stack);
		if (inst->base.push)
		    STACK_PUSH (thread->thread.continuation.stack, value);
		thread->thread.continuation.pc = next;
		if (thread->thread.next)
		    ThreadStepped (thread);
	    }
	    else
	    {
		/*
		 * Check for pending execution signals
		 */
		if (signalSuspend)
		{
		    signalSuspend = False;
		    ThreadSetState (thread, ThreadSuspended);
		}
		if (signalFinished)
		{
		    signalFinished = False;
		    ThreadFinish (thread);
		}
		if (signalException)
		{
		    signalException = False;
		    JumpStandardException (thread, &next);
		    thread->thread.continuation.pc = next;
		}
		if (signalError)
		{
		    signalError = False;
		    DebugStart (NewContinuation (&thread->thread.continuation,
						 inst));
		    ThreadFinish (thread);
		}
	    }
	    EXIT ();
	}
	else
	{
	    sigset_t	set, oset;

	    sigfillset (&set);
	    sigprocmask (SIG_SETMASK, &set, &oset);
	    if (!signaling)
		sigsuspend (&oset);
	    sigprocmask (SIG_SETMASK, &oset, &set);
	}
    }
}
