/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

#define Stack(i) ((Value) STACK_ELT(thread->thread.stack, i))

/*
 * Instruction must be completed because changes have been
 * committed to storage.
 */

Bool	complete;

Bool	signalFinished;	    /* current thread is finished */
Bool	signalSuspend;	    /* current thread is suspending */

static ThreadState ThreadStep (Value thread);

#define Arg(n)  Stack((argc - 1) - (n) + off)

static FramePtr
BuildFrame (Value thread, Value func, Bool staticInit, Bool tail,
	    Bool varargs, int nformal, int off,
	    int argc, InstPtr savePc)
{
    ENTER ();
    CodePtr	    code = func->func.code;
    FuncBodyPtr	    body = staticInit ? &code->func.staticInit : &code->func.body;
    int		    fe;
    FramePtr	    frame;
    
    frame = thread->thread.frame;
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
	frame->savePc = thread->thread.frame->savePc;
	frame->saveCode = thread->thread.frame->saveCode;
    }
    else
    {
	frame->savePc = savePc;
	frame->saveCode = thread->thread.code;
    }
    RETURN (frame);
}

static Value
ThreadCall (Value thread, Bool tail, InstPtr *next, int *stack)
{
    ENTER ();
    Value	value = thread->thread.v;
    CodePtr	code = value->func.code;
    FramePtr	frame;
    ArgType	*argt;
    int		argc = *stack;
    int		fe;
    int		off = 0;
    
    if (argc == -1)
    {
	off = 1;
	argc = Stack(0)->ints.value;
	*stack = argc+1;
    }
	
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
	    *next = thread->thread.frame->savePc;
	    thread->thread.code = thread->thread.frame->saveCode;
	    thread->thread.frame = thread->thread.frame->previous;
	}
    }
    else
    {
	frame = BuildFrame (thread, value, False, tail, code->base.varargs,
			    code->base.argc, off, argc, *next);
	if (aborting)
	    RETURN (value);
	complete = True;
	thread->thread.frame = frame;
	thread->thread.code = code->func.body.obj;
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
    
    frame = BuildFrame (thread, value, True, False, False, 0, 0, 0, *next);
    if (aborting)
	return;
    complete = True;
    thread->thread.frame = frame;
    thread->thread.code = code->func.staticInit.obj;
    *next = ObjCode (thread->thread.code, 0);
    EXIT ();
}

static void
ThreadAssign (Value ref, Value v, Bool initialize)
{
    ENTER ();
    if (RefConstant(ref) && !initialize)
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
	if (aborting)
	    break;
	if (part < 0 || a->array.dim[dim] <= part)
	{
	    RaiseStandardException (exception_invalid_array_bounds,
				    "Array index out of bounds",
				    2, a, Stack(dim));
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

static void
ThreadRaise (Value thread, int argc, SymbolPtr exception, InstPtr *next)
{
    ENTER ();
    BoxPtr	args = 0;
    int i;

#ifdef DEBUG_JUMP
    FilePrintf (FileStdout, "    Raise: %A\n", exception->symbol.name);
#endif
    args = NewBox (True, False, argc);
    for (i = 0; i < argc; i++)
        BoxValueSet (args, i, STACK_POP (thread->thread.stack));
    (void) RaiseException (thread, exception, args, next);
    EXIT ();
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

static void
ThreadUnwind (Value thread, int twixts, int catches, InstPtr *next)
{
    ENTER ();
    Value	continuation;
    CatchPtr	catch;
    TwixtPtr	twixt;
    
    twixt = thread->thread.twixts;
    while (twixts--)
	twixt = twixt->previous;
    catch = thread->thread.catches;
    while (catches--)
	catch = catch->previous;
    
    /*
     * Create a continuation that unwinds the twixts
     * and catches and then ends up executing the next
     * instruction
     */
    continuation = NewContinuation (thread->thread.frame,
				    *next,
				    running->thread.stack,
				    catch, twixt);
    (void) do_longjmp (next, continuation, Zero);
    EXIT ();
}

static void
ThreadBoxCheck (BoxPtr box, int i, Types *type)
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

#ifdef HAVE_C_INLINE
static inline ThreadState
#else
static ThreadState
#endif
ThreadStep (Value thread)
{
    ENTER ();
    InstPtr	inst, next;
    FramePtr	fp;
    int		i;
    int		stack = 0;
    Value	value, v, w;
    BoxPtr	box;
    
    inst = thread->thread.pc;
    value = thread->thread.v;
    next = inst + 1;
    complete = False;
/*    InstDump (inst, 1, 0, 0, 0);*/
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
	    RaiseError ("break/continue outside of loop/switch/twixt");
	    break;
	}
	next = inst + inst->branch.offset;
	break;
    case OpDo:
    case OpOr:
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
    case OpDefault:
        next = inst + inst->branch.offset;
	stack++;
	break;
    case OpTagDefault:
        next = inst + inst->branch.offset;
	break;
    case OpEnd:
	SetSignalFinished ();
	break;
    case OpReturnVoid:
	value = Void;
	/* fall through */
    case OpReturn:
	if (!thread->thread.frame)
	{
	    RaiseError ("return outside of function");
	    break;
	}
	if (!TypeCompatibleAssign (thread->thread.frame->function->func.code->base.type,
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
	next = thread->thread.frame->savePc;
	thread->thread.code = thread->thread.frame->saveCode;
	thread->thread.frame = thread->thread.frame->previous;
	break;
    case OpName:
    case OpNameRef:
    case OpNameRefStore:
	switch (inst->var.name->symbol.class) {
	case class_global:
	case class_const:
	    box = inst->var.name->global.value;
	    i = 0;
	    break;
	case class_static:
	    for (i = 0, fp = thread->thread.frame; i < inst->var.staticLink; i++)
		fp = fp->staticLink;
	    box = fp->statics;
	    i = inst->var.name->local.element;
	    break;
	case class_auto:
	case class_arg:
	    for (i = 0, fp = thread->thread.frame; i < inst->var.staticLink; i++)
		fp = fp->staticLink;
	    box = fp->frame;
	    i = inst->var.name->local.element;
	    break;
	default:
	    RaiseError ("Invalid symbol class %C for lvalue \"%A\"",
			inst->var.name->symbol.class,
			inst->var.name->symbol.name);
	    box = 0;
	    i = 0;
	    break;
	}
	if (!box)
	    break;
	if (inst->base.opCode != OpNameRefStore)
	    ThreadBoxCheck (box, i, inst->var.name->symbol.type);
	if (inst->base.opCode == OpName)
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
	if (aborting)
	    break;
	complete = True;
	next = thread->thread.frame->savePc;
	thread->thread.code = thread->thread.frame->saveCode;
	thread->thread.frame = thread->thread.frame->previous;
	/* Fetch the Obj from the stack */
	value = Stack (stack); stack++;
	break;
    case OpStar:
	if (!ValueIsRef(value))
	{
	    RaiseStandardException (exception_invalid_unop_value,
				    "Not a reference",
				    1, value);
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
	if (!ValueIsRef(value))
	{
	    RaiseStandardException (exception_invalid_unop_value,
				    "Not an lvalue",
				    1, value);
	    break;
	}
	v = RefValue (value);
	if (inst->base.opCode == OpPreInc || inst->base.opCode == OpPostInc)
	    w = Plus (v, One);
	else
	    w = Minus (v, One);
	ThreadAssign (value, w, False);
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
	if (!ValueIsRef(value))
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
	ThreadAssign (value, v, False);
	value = v;
	break;
    case OpInitialize:
	if (!ValueIsRef(value))
	    break;
	v = Stack(stack); stack++;
	ThreadAssign (value, v, True);
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
	if (aborting)
	    break;
	ThreadCatch (thread, inst->catch.exception, inst->catch.offset);
	complete = True;
	next = inst + inst->catch.offset;
	break;
    case OpException:
	next = inst + inst->branch.offset;
	break;
    case OpEndCatch:
	if (aborting)
	    break;
	thread->thread.catches = thread->thread.catches->previous;
	complete = True;
	break;
    case OpRaise:
	if (aborting)
	    break;
	ThreadRaise (thread, inst->raise.argc, inst->raise.exception, &next);
	if (!aborting)
	    complete = True;
	break;
    case OpTwixt:
	if (aborting)
	    break;
	ThreadTwixt (thread, inst->twixt.enter, inst->twixt.leave);
	complete = True;
	break;
    case OpTwixtDone:
	if (aborting)
	    break;
	thread->thread.twixts = thread->thread.twixts->previous;
	complete = True;
	break;
    case OpEnterDone:
	if (!True (value))
	{
	    next = inst + inst->branch.offset;
	    thread->thread.jump = 0;
	}
	else
	{
	    if (aborting)
		break;
	    if (thread->thread.jump)
	    {
		value = JumpContinuation (thread->thread.jump, &next);
		complete = True;
	    }
	}
	break;
    case OpLeaveDone:
	if (thread->thread.jump)
	{
	    if (aborting)
		break;
	    value = JumpContinuation (thread->thread.jump, &next);
	    complete = True;
	}
	else
	    next = inst + inst->branch.offset;
	break;
    case OpUnwind:
	if (aborting)
	    break;
	ThreadUnwind (thread, inst->unwind.twixt, inst->unwind.catch, &next);
	complete = True;
	break;
    }
    if (!aborting || complete)
    {
	/* this instruction has been completely executed */
	thread->thread.partial = 0;
	thread->thread.v = value;
	if (stack)
	    STACK_DROP (thread->thread.stack, stack);
	if (inst->base.push)
	    STACK_PUSH (thread->thread.stack, value);
	thread->thread.pc = next;
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
	    thread->thread.pc = next;
	}
	if (signalError)
	{
	    signalError = False;
	    DebugStart (NewContinuation (thread->thread.frame,
					 inst,
					 thread->thread.stack,
					 thread->thread.catches,
					 thread->thread.twixts));
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
	    ThreadStep (running);
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
