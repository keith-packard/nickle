/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	<assert.h>

#undef DEBUG_FRAME

#define Stack(i) ((Value) STACK_ELT(thread->thread.continuation.stack, i))

/*
 * Instruction must be completed because changes have been
 * committed to storage.
 */

Bool	complete;

Bool	signalFinished;	    /* current thread is finished */
Bool	signalSuspend;	    /* current thread is suspending */

#define Arg(n)	((n) <= base ? Stack(base - (n)) : value)

static FramePtr
BuildFrame (Value thread, Value func, Value value, Bool staticInit, Bool tail,
	    Bool varargs, int nformal,
	    int base, int argc, InstPtr savePc, ObjPtr saveObj)
{
    ENTER ();
    CodePtr	    code = func->func.code;
    FuncBodyPtr	    body = staticInit ? &code->func.staticInit : &code->func.body;
    int		    fe;
    FramePtr	    frame;
    
#ifdef DEBUG_FRAME
    FilePrintf (FileStdout, "BuildFrame func %v value %v\n", func, value);
    ThreadStackDump (thread);
    FileFlush (FileStdout, True);
#endif
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
	
	array = NewArray (True, typePoly, 1, &extra);
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
    int		argc = *stack;
    Value	value = thread->thread.continuation.value;
    Value	func;
    CodePtr	code;
    FramePtr	frame;
    ArgType	*argt;
    int		fe;
    int		base;
    
    /*
     * Typecheck actuals
     */
    fe = 0;
    base = argc - 2;
    if (argc < 0)
    {
	Value	numvar = Arg(0);
	
	if (!ValueIsInt (numvar))
	{
	    RaiseStandardException (exception_invalid_argument, 
				    "Incompatible argument",
				    2, NewInt(-1), Arg(0));
	    RETURN (Void);
	}
	argc = -argc - 1 + numvar->ints.value;
	base = argc - 1;
	*stack = 1 + argc;  /* count + args */
	func = Stack(argc);
    }
    else
	func = argc ? Stack(argc-1) : value;
    if (!ValueIsFunc(func))
    {
	ThreadStackDump (thread);
	RaiseStandardException (exception_invalid_unop_value,
				"Not a function",
				1, func);
	RETURN (Void);
    }
    code = func->func.code;
    argt = code->base.args;
    while (fe < argc || (argt && !argt->varargs))
    {
	if (!argt)
	{
	    RaiseStandardException (exception_invalid_argument, 
				    "Too many parameters",
				    2, NewInt (argc), NewInt(code->base.argc));
	    RETURN (Void);
	}
	if (fe == argc)
	{
	    RaiseStandardException (exception_invalid_argument, 
				    "Too few arguments",
				    2, NewInt (argc), NewInt(code->base.argc));
	    RETURN (Void);
	}
	if (!TypeCompatibleAssign (argt->type, Arg(fe)))
	{
	    RaiseStandardException (exception_invalid_argument, 
				    "Incompatible argument",
				    2, NewInt (fe), Arg(fe));
	    RETURN (Void);
	}
	fe++;
	if (!argt->varargs)
	    argt = argt->next;
    }

    if (code->base.builtin)
    {
	Value	*values = 0;
	int	formal;

	formal = code->base.argc;
	if (code->base.varargs)
	{
	    formal = -1;
	    values = AllocateTemp (argc * sizeof (Value));
	    for (fe = 0; fe < argc; fe++)
		values[fe] = Arg(fe);
	}

	if (code->builtin.needsNext) 
	{
	    /*
	     * Let the non-local function handle the stack adjust
	     */
	    *stack = 0;
	    switch (formal) {
	    case -1:
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
		value = Void;
		break;
	    }
	}
	if (tail && !aborting)
	{
	    complete = True;
	    thread->thread.continuation.obj = thread->thread.continuation.frame->saveObj;
	    *next = thread->thread.continuation.frame->savePc;
	    thread->thread.continuation.frame = thread->thread.continuation.frame->previous;
	}
    }
    else
    {
	frame = BuildFrame (thread, func, value, False, tail, code->base.varargs,
			    code->base.argc, base, argc, *next, thread->thread.continuation.obj);
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
    
    frame = BuildFrame (thread, value, Void, True, False, False, 0, 0, 0,
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
ThreadArray (Value thread, int ndim, Type *type)
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
ThreadArrayIndex (Value array, Value thread, int ndim, 
		  Value last, int off, Bool except)
{
    int	    i;
    int	    dim;
    int	    part;
    Value   d;
    
    i = 0;
    for (dim = ndim - 1; dim >= 0; dim--)
    {
	if (dim == 0)
	    d = last;
	else
	    d = Stack(dim + off - 1);
	if (!ValueIsInt(d) || 
	    (((part = d->ints.value) < 0 || 
	      array->array.dim[dim] <= part) && except))
	{
	    RaiseStandardException (exception_invalid_array_bounds,
				    "Array index out of bounds",
				    2, array, Stack(dim + off - 1));
	    return 0;
	}
	i = i * array->array.dim[dim] + part;
    }
    return i;
}

/*
 * Array initialization
 *
 * Array initialization uses the stack to hold temporary index values while
 * the array is walked.  The stack looks like:
 *
 *	array
 *	major-index
 *	...
 *	minor-index
 *	num-dimensions
 *
 * Each Element Repeat instruction indicates which dimension
 * is being stepped over.  When the entire array has been walked,
 * a final Element inst with ndim set to the dimensionality of the
 * array is executed which pops the whole mess off the stack and
 * returns the completed array
 *
 * Repeat elements indicate along which dimension the repeat occurs.
 * The initial value for the repeat has already been stored in the
 * array, so the task is just to copy that first element to the end
 * of the dimension.
 */
 
static void
ThreadArrayReplicate (Value thread, Value array, int dim, int start)
{
    int		dimsize = 1;
    int		i;
    int		total;
    BoxElement	*elements;

    for (i = 0; i < dim; i++)
	dimsize *= array->array.dim[i];
    start = start - dimsize;
    total = array->array.dim[dim] * dimsize;
    elements = BoxElements (array->array.values);
    for (i = start + dimsize; i % total; i += dimsize)
	memmove (elements + i, elements + start,
		 dimsize * sizeof (elements[0]));
}

void
ThreadStackDump (Value thread);
    
static Value
ThreadArrayInit (Value thread, Value value, AInitMode mode, 
		 int dim, int *stack, InstPtr *next)
{
    ENTER ();
    Value   array;
    int	    i;
    int	    ndim;

    if (aborting)
	RETURN(value);
    switch (mode) {
    case AInitModeStart:
	complete = True;
	STACK_PUSH (thread->thread.continuation.stack, value);
	for (i = 0; i < dim; i++)
	    STACK_PUSH (thread->thread.continuation.stack, Zero);
	STACK_PUSH (thread->thread.continuation.stack, NewInt(dim));
	break;
    case AInitModeRepeat:
        ndim = Stack(0)->ints.value;
	array = Stack(ndim+1);
	if (Stack(dim+1)->ints.value == array->array.dim[dim])
	    break;
	/* fall through ... */
    case AInitModeElement:
        ndim = Stack(0)->ints.value;
	array = Stack(ndim+1);
	/*
	 * Check and see if we're done
	 */
	if (dim == ndim)
	{
	    *stack = ndim + 2;
	    value = array;
	    break;
	}
	/*
	 * Initialize a single value?
	 */
	if (dim == 0)
	{
	    if (!TypeCompatibleAssign (array->array.type, value))
	    {
		RaiseStandardException (exception_invalid_argument,
				"Incompatible types in array initialization",
				2, array, value);
		break;
	    }
	    i = ThreadArrayIndex (array, thread, ndim, Stack(1), 2, True);
	    if (aborting)
		break;
	    complete=True;
	    BoxValueSet (array->array.values, i, Copy (value));
	}
	complete = True;
	/*
	 * Step to the next element
	 */
	STACK_DROP(thread->thread.continuation.stack, dim+1);
	STACK_PUSH(thread->thread.continuation.stack,
		   Plus (STACK_POP(thread->thread.continuation.stack),
				   One));
	/*
	 * Reset remaining indices to zero
	 */
	for (i = 0; i < dim; i++)
	    STACK_PUSH (thread->thread.continuation.stack, Zero);
	STACK_PUSH (thread->thread.continuation.stack, NewInt (ndim));
	if (mode == AInitModeRepeat)
	{
	    i = ThreadArrayIndex (array, thread, ndim, Stack(1), 2, False);
	    ThreadArrayReplicate (thread, array, dim, i);
	}
	break;
    case AInitModeFunc:
	if (aborting)
	    break;
	complete = True;
	/*
	 * Fetch the function
	 */
	value = Stack(dim+2);
	/*
	 * Push args. Tricky because the stack keeps growing
	 */
	i = dim + 1;
	while (--dim >= 0)
	{
	    STACK_PUSH(thread->thread.continuation.stack, value);
	    value = Stack(i);
	}
	break;
    case AInitModeFuncDone:
        ndim = Stack(0)->ints.value;
	value = Stack(ndim+1);
	*stack = ndim + 3;
	break;
    case AInitModeTest:
	if (aborting)
	    break;
	complete = True;
        ndim = Stack(0)->ints.value;
	array = Stack(ndim+1);
	value = FalseVal;
	/* Done with this row? */
	if (Stack(1)->ints.value == array->array.dim[0])
	{
	    /* Find dim with space */
	    for (dim = 1; dim < ndim; dim++)
		if (Stack(1+dim)->ints.value < array->array.dim[dim] - 1)
		    break;
	    /* All done? */
	    if (dim == ndim)
	    {
		value = TrueVal;
		break;
	    }
	    /*
	     * Step to the next element
	     */
	    STACK_DROP(thread->thread.continuation.stack, dim+1);
	    STACK_PUSH(thread->thread.continuation.stack,
		       Plus (STACK_POP(thread->thread.continuation.stack),
			     One));
	    /*
	     * Reset remaining indices to zero
	     */
	    while (--dim >= 0)
		STACK_PUSH (thread->thread.continuation.stack, Zero);
	    STACK_PUSH (thread->thread.continuation.stack, NewInt (ndim));
	}
	break;
    }
    RETURN (value);
}


static Value
ThreadRaise (Value thread, Value value, int argc, SymbolPtr exception, InstPtr *next, int *stack)
{
    ENTER ();
    Value   args;
    int i;
    int base = argc - 2;

#ifdef DEBUG_JUMP
    FilePrintf (FileStdout, "    Raise: %A\n", exception->symbol.name);
#endif
    /*
     * Build and array to hold the arguments, this will end up
     * in the thread's value on entry to the catch block
     */
    args = NewArray (False, typePoly, 1, &argc);
    for (i = 0; i < argc; i++)
        BoxValueSet (args->array.values, i, Arg(i));
    *stack = argc ? argc - 1 : 0;
    if (!aborting)
    {
	complete = True;
	RaiseException (thread, exception, args, next);
    }
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
	RETURN (Void);
    }
    if (aborting)
    {
	*stack = 1;
	RETURN (Void);
    }
    complete = True;
    argc = args->array.ents;
    i = argc;
    while (--i >= 0)
    {
	STACK_PUSH (thread->thread.continuation.stack,
		    thread->thread.continuation.value);
	thread->thread.continuation.value = BoxValue(args->array.values, i);
    }
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
ThreadFarJump (Value thread, Value ret, FarJumpPtr farJump, InstPtr *next)
{
    ENTER ();
    Value	continuation;

    /*
     * Build the continuation
     */
    continuation = FarJumpContinuation (&thread->thread.continuation, farJump);
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

#define ThreadBoxCheck(box,i,type) (BoxValueGet(box,i) == 0 ? ThreadBoxSetDefault(box,i,type,0) : 0)

typedef struct _TypeChain {
    struct _TypeChain	*prev;
    Type   *type;
} TypeChain;

static void
ThreadBoxSetDefault (BoxPtr box, int i, Type *type, TypeChain *chain)
{
    if (BoxValueGet (box, i) == 0)
    {
	Type	    *ctype = TypeCanon (type);
	StructType  *st = ctype->structs.structs;
	TypeChain   link, *c;

	/*
	 * Check for recursion
	 */
	for (c = chain; c; c = c->prev)
	    if (c->type == ctype)
		return;

	link.prev = chain;
	link.type = ctype;
	
	switch (ctype->base.tag) {
	case type_union:
	    BoxValueSet (box, i, NewUnion (st, False));
	    break;
	case type_struct:
	    BoxValueSet (box, i, NewStruct (st, False));
	    box = BoxValueGet (box, i)->structs.values;
	    for (i = 0; i < st->nelements; i++)
	    {
		if (BoxValueGet (box, i) == 0)
		    ThreadBoxSetDefault (box, i, StructTypeElements(st)[i].type,
					&link);
	    }
	    break;
	default:
	    break;
	}
    }
}

#include	<signal.h>

void
ThreadStackDump (Value thread)
{
    StackObject	    *stack = thread->thread.continuation.stack;
    StackChunk	    *chunk;
    StackPointer    stackPointer;
    int		    i = 0;

    chunk = stack->current;
    stackPointer = STACK_TOP(stack);
    while (chunk)
    {
	while (stackPointer < CHUNK_MAX(chunk))
	{
	    FilePrintf (FileStdout, "%d: %v\n", i++, (Value) *stackPointer++);
	}
	chunk = chunk->previous;
	stackPointer = CHUNK_MIN(chunk);
    }
}

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
	    if (lex && !(lex->file.flags & (FileInputBlocked|FileOutputBlocked)))
		break;
	}
	else if (thread && !Runnable (thread))
	    break;
	else if (running)
	{
	    ENTER ();
	    Value	thread = running;
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
#ifdef DEBUG_INST
	    InstDump (inst, 1, 0, 0, 0);
	    FileFlush (FileStdout, True);
#endif
	    switch (inst->base.opCode) {
	    case OpNoop:
		break;
	    case OpBranch:
		next = inst + inst->branch.offset;
		break;
	    case OpBranchFalse:
		if (!ValueIsBool(value))
		{
		    RaiseStandardException (exception_invalid_argument,
					    "conditional expression not bool",
					    2, value, Void);
		    break;
		}
		if (!True (value))
		    next = inst + inst->branch.offset;
		break;
	    case OpBranchTrue:
		if (!ValueIsBool(value))
		{
		    RaiseStandardException (exception_invalid_argument,
					    "conditional expression not bool",
					    2, value, Void);
		    break;
		}
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
					    2, value, Void);
		    break;
		}
		if (value->unions.tag == inst->tagcase.tag)
		{
		    next = inst + inst->tagcase.offset;
		    v = UnionValue (value, inst->tagcase.tag);
		    if (!v)
		    {
			if (StructMemType (value->unions.type, inst->atom.atom))
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
		}
		break;
	    case OpTagStore:
		box = 0;
		if (inst->var.name->symbol.class == class_global)
		{
		    box = inst->var.name->global.value;
		    i = 0;
		}
		else
		{
		    box = thread->thread.continuation.frame->frame;
		    i = inst->var.name->local.element;
		}
		if (!box)
		    break;
		ThreadAssign (NewRef (box, i), value, True);
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
					    2, value, Void);
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
	    case OpFetch:
		value = Dereference (value);
		break;
	    case OpConst:
		value = inst->constant.constant;
		break;
	    case OpBuildArray:
		stack = inst->array.ndim;
		value = ThreadArray (thread, stack, inst->array.type);
		break;
	    case OpInitArray:
		stack = 0;
		value = ThreadArrayInit (thread, value, inst->ainit.mode,
					 inst->ainit.dim, &stack, &next);
		break;
	    case OpBuildStruct:
		value = NewStruct (inst->structs.structs, False);
		break;
	    case OpInitStruct:
		w = Stack(stack); stack++;
		v = StructMemRef (w, inst->atom.atom);
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
		stack = inst->ints.value;
		v = Stack(stack-1);
		switch (ValueTag(v)) {
		    char    *s;
		case rep_string:
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
		    i = IntPart (value, "Invalid string index");
		    if (aborting)
			break;
		    s = StringChars (&v->string);
		    if (i < 0 || StringLength (s) < i)
		    {
			RaiseStandardException (exception_invalid_binop_values,
						"String index out of bounds",
						2, value, v);
			break;
		    }
		    value = NewInt (StringGet (s, i));
		    break;
		case rep_array:
		    if (inst->ints.value != v->array.ndim)
		    {
			RaiseStandardException (exception_invalid_binop_values,
						"Mismatching dimensionality",
						2, NewInt (inst->ints.value), v);
			break;
		    }
		    stack = inst->ints.value;
		    i = ThreadArrayIndex (v, thread, stack, value, 0, True);
		    if (!aborting)
		    {
			if (inst->base.opCode != OpArrayRefStore)
			    ThreadBoxCheck (v->array.values, i, v->array.type);
			if (inst->base.opCode == OpArray)
			    value = BoxValue (v->array.values, i);
			else
			    value = NewRef (v->array.values, i);
		    }
		    break;
		default:
		    RaiseStandardException (exception_invalid_unop_value,
					    "Not an array",
					    1, value);
		    break;
		}
		break;
	    case OpVarActual:
		if (!ValueIsArray(value))
		{
		    RaiseStandardException (exception_invalid_unop_value,
					    "Not an array",
					    1, value);
		    break;
		}
		if (value->array.ndim != 1)
		{
		    RaiseStandardException (exception_invalid_unop_value,
					    "Array not one dimension",
					    1, value);
		    break;
		}
		for (i = 0; i < value->array.dim[0]; i++)
		{
		    STACK_PUSH (thread->thread.continuation.stack,
				BoxValue (value->array.values, i));
		    if (aborting)
		    {
			STACK_DROP (thread->thread.continuation.stack,
				    i + 1);
			break;
		    }
		}
		if (i != value->array.dim[0])
		    break;
		complete = True;
		value = NewInt (value->array.dim[0]);
		break;
	    case OpCall:
	    case OpTailCall:
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
		case rep_struct:
		    if (inst->base.opCode == OpDot || inst->base.opCode == OpArrow)
			v = StructMemValue (value, inst->atom.atom);
		    else
			v = StructMemRef (value, inst->atom.atom);
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
		case rep_union:
		    if (inst->base.opCode == OpDot || inst->base.opCode == OpArrow)
			v = UnionValue (value, inst->atom.atom);
		    else
		    {
			v = UnionRef (value, inst->atom.atom);
		    }
		    if (!v)
		    {
			if (StructMemType (value->unions.type, inst->atom.atom))
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
		ThreadAssign (Stack(stack), value, inst->assign.initialize); stack++;
		break;
	    case OpAssignOp:
		v = BinaryOperate (Stack(stack), value, inst->binop.op); stack++;
		ThreadAssign (Stack(stack), v, False); stack++;
		value = v;
		break;
	    case OpAssignFunc:
		v = (*inst->binfunc.func) (Stack(stack), value); stack++;
		ThreadAssign (Stack(stack), v, False); stack++;
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
		stack = 0;
		value = ThreadRaise (thread, value, inst->raise.argc, inst->raise.exception, &next, &stack);
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
		if (thread->thread.jump)
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
		break;
	    case OpFarJump:
		ThreadFarJump (thread, value, inst->farJump.farJump, &next);
		break;
	    case OpEnd:
		SetSignalFinished ();
		break;
	    default:
		break;
	    }
#if 0
	    assert (!next || 
		    (ObjCode (thread->thread.continuation.obj, 0) <= next &&
		     next <= ObjCode (thread->thread.continuation.obj,
				      thread->thread.continuation.obj->used)));
#endif
	    if (signalProfile)
	    {
		signalProfile = False;
		ProfileInterrupt (running);
	    }
	    if (!aborting || complete)
	    {
		/* this instruction has been completely executed */
		thread->thread.partial = 0;
		thread->thread.continuation.value = value;
		if (stack)
		    STACK_DROP (thread->thread.continuation.stack, stack);
		if (inst->base.flags & InstPush)
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
		    thread->thread.continuation.value = JumpStandardException (thread, &next);
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
