/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	<assert.h>

#undef DEBUG_FRAME

#define Stack(i) ((Value) STACK_ELT(thread->thread.continuation.stack, i))
#define CStack(i)   ((Value) STACK_ELT(cstack, i))

/*
 * Instruction must be completed because changes have been
 * committed to storage.
 */

Bool	complete;

Bool	signalFinished;	    /* current thread is finished */
Bool	signalSuspend;	    /* current thread is suspending */

#define Arg(n)	((n) <= base ? CStack(base - (n)) : value)

static FramePtr
BuildFrame (Value thread, Value func, Value value, Bool staticInit, Bool tail,
	    Bool varargs, int nformal,
	    int base, int argc, InstPtr savePc, ObjPtr saveObj)
{
    ENTER ();
    CodePtr	    code = func->func.code;
    FuncBodyPtr	    body = staticInit ? &code->func.staticInit : &code->func.body;
    StackObject	    *cstack = thread->thread.continuation.stack;
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
	
	array = NewArray (True, False, typePoly, 1, &extra);
	BoxValueSet (frame->frame, fe, array);
	for (; fe < argc; fe++)
	    ArrayValueSet (&array->array, fe-nformal, Copy (Arg(fe)));
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
    StackObject *cstack = thread->thread.continuation.stack;
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
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("Incompatible argument"),
				    NewInt(-1), Arg(0));
	    RETURN (Void);
	}
	argc = -argc - 1 + ValueInt(numvar);
	base = argc - 1;
	*stack = 1 + argc;  /* count + args */
	func = CStack(argc);
    }
    else
	func = argc ? CStack(argc-1) : value;
    if (!ValueIsFunc(func))
    {
	ThreadStackDump (thread);
	RaiseStandardException (exception_invalid_unop_value, 1, func);
	RETURN (Void);
    }
    code = func->func.code;
    argt = code->base.args;
    while (fe < argc || (argt && !argt->varargs))
    {
	if (!argt)
	{
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("Too many arguments"),
				    NewInt (argc), NewInt(code->base.argc));
	    RETURN (Void);
	}
	if (fe == argc)
	{
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("Too few arguments"),
				    NewInt (argc), NewInt(code->base.argc));
	    RETURN (Void);
	}
	if (!TypeCompatibleAssign (argt->type, Arg(fe)))
	{
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("Incompatible argument"),
				    NewInt (fe), Arg(fe));
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
	    /*
	     * For a tail call, drop the topmost frame
	     */
	    if (tail && !aborting)
	    {
		complete = True;
		thread->thread.continuation.obj = thread->thread.continuation.frame->saveObj;
		*next = thread->thread.continuation.frame->savePc;
		thread->thread.continuation.frame = thread->thread.continuation.frame->previous;
	    }
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

static inline void
ThreadAssign (Value ref, Value v, Bool initialize)
{
    ENTER ();
    if (!ValueIsRef (ref))
	RaiseStandardException (exception_invalid_binop_values, 2, ref, v);
    else if (RefConstant(ref) && !initialize)
	RaiseStandardException (exception_readonly_box, 1, v);
    else if (ref->ref.element >= ref->ref.box->nvalues)
	RaiseStandardException (exception_invalid_array_bounds, 2,
				NewInt(ref->ref.element), v);
    else if (!TypeCompatibleAssign (RefType (ref), v))
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("Incompatible types in assignment"),
				NewInt(ref->ref.element), v);
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
ThreadArray (Value thread, Bool resizable, int ndim, Type *type)
{
    ENTER ();
    int	    i;
    int	    *dims;

    dims = AllocateTemp (ndim * sizeof (int));
    for (i = 0; i < ndim; i++)
    {
	Value	d = Stack(i);
	dims[i] = IntPart (d, "Invalid array dimension");
	if (dims[i] < 0)
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("Negative array dimension"),
				    NewInt (0), d);
	if (aborting)
	    RETURN (0);
    }
    RETURN (NewArray (False, resizable, type, ndim, dims));
}

static Value
ThreadArrayInd (Value thread, Bool resizable, Value dim, Type *type)
{
    ENTER ();
    Array   *a = &dim->array;
    int	    i;
    int	    ndim = ArrayLimits(a)[0];
    int	    *dims;

    dims = AllocateTemp (ndim * sizeof (int));
    for (i = 0; i < ndim; i++)
    {
	Value	d = ArrayValue (a, i);
	dims[i] = IntPart (d, "Invalid array dimension");
	if (dims[i] < 0)
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("Negative array dimension"), 
				    NewInt (0), d);
	if (aborting)
	    RETURN (0);
    }
    RETURN (NewArray (False, resizable, type, ndim, dims));
}

static int
ThreadArrayIndex (Value array, Value thread, int ndim, 
		  Value last, int off, Bool except, Bool resize)
{
    int	    i;
    int	    dim;
    int	    part;
    Value   d;
    int	    *dims = ArrayDims (&array->array);
    int	    *limits = ArrayLimits (&array->array);
    
    i = 0;
    for (dim = ndim - 1; dim >= 0; dim--)
    {
	if (dim == 0)
	    d = last;
	else
	    d = Stack(dim + off - 1);
	if (!ValueIsInt(d) || (part = ValueInt(d)) < 0)
	{
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("Array index not non-negative integer"),
				    array, d);
	    return 0;
	}
	if (limits[dim] <= part)
	{
	    if (resize && array->array.resizable)
	    {
		if (dims[dim] > part)
		    limits[dim] = part + 1;
		else
		    ArrayResize (array, dim, part + 1);
	    }
	    else if (except)
	    {
		RaiseStandardException (exception_invalid_array_bounds, 2,
					array, d);
		return 0;
	    }
	}
	i = i * dims[dim] + part;
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
    Value	*elements;

    for (i = 0; i < dim; i++)
	dimsize *= ArrayDims(&array->array)[i];
    start = start - dimsize;
    total = ArrayDims(&array->array)[dim] * dimsize;
    if (array->array.resizable) {
	for (i = start + dimsize; i % total; i += dimsize) {
	    int j;
	    for (j = 0; j < dimsize; j++)
		ArrayValueSet(&array->array, i + j, ArrayValueGet(&array->array, start + j));
	}
    } else {
	elements = BoxElements (array->array.u.fix);
	for (i = start + dimsize; i % total; i += dimsize)
	    memmove (elements + i, elements + start,
		     dimsize * sizeof (elements[0]));
    }
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
        ndim = ValueInt(Stack(0));
	array = Stack(ndim+1);
	if (ValueInt(Stack(dim+1)) == ArrayDims(&array->array)[dim])
	    break;
	/* fall through ... */
    case AInitModeElement:
        ndim = ValueInt(Stack(0));
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
	    if (!TypeCompatibleAssign (ArrayType(&array->array), value))
	    {
		RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("Incompatible types in array initialization"),
				array, value);
		break;
	    }
	    i = ThreadArrayIndex (array, thread, ndim, Stack(1), 2, True, False);
	    if (aborting)
		break;
	    complete=True;
	    ArrayValueSet (&array->array, i, Copy (value));
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
	    i = ThreadArrayIndex (array, thread, ndim, Stack(1), 2, False, False);
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
        ndim = ValueInt(Stack(0));
	value = Stack(ndim+1);
	*stack = ndim + 3;
	break;
    case AInitModeTest:
	if (aborting)
	    break;
	complete = True;
        ndim = ValueInt(Stack(0));
	array = Stack(ndim+1);
	value = FalseVal;
	/* Done with this row? */
	if (ValueInt(Stack(1)) == ArrayDims(&array->array)[0])
	{
	    /* Find dim with space */
	    for (dim = 1; dim < ndim; dim++)
		if (ValueInt(Stack(1+dim)) < ArrayDims(&array->array)[dim] - 1)
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


#ifdef DEBUG_JUMP
void
ThreadCatches (Value thread)
{
    CatchPtr	catch;

    FilePrintf (FileStdout, "(");
    for (catch = thread->thread.continuation.catches; 
	 catch;
	 catch = catch->continuation.catches)
    {
	FilePrintf (FileStdout, "%A ", catch->exception->symbol.name);
    }
    FilePrintf (FileStdout, ")\n");
}
#endif

static Value
ThreadRaise (Value thread, Value value, int argc, SymbolPtr exception, InstPtr *next)
{
    ENTER ();
    StackObject *cstack = thread->thread.continuation.stack;
    Value	args;
    int		i;
    int		base = argc - 2;

#ifdef DEBUG_JUMP
    FilePrintf (FileStdout, "    Raise: %A ", exception->symbol.name);
    ThreadCatches (thread);
#endif
    /*
     * Build and array to hold the arguments, this will end up
     * in the thread's value on entry to the catch block
     */
    args = NewArray (False, False, typePoly, 1, &argc);
    for (i = 0; i < argc; i++)
        ArrayValueSet (&args->array, i, Arg(i));
    if (!aborting)
    {
	RaiseException (thread, exception, args, next);
	if (!aborting)
	    complete = True;
    }
    RETURN (args);
}

static void
ThreadEndCatch (Value thread, int catches)
{
#ifdef DEBUG_JUMP
    FilePrintf (FileStdout, "    EndCatch: %d ", catches);
#endif
    while (catches--)
        thread->thread.continuation.catches = thread->thread.continuation.catches->continuation.catches;
#ifdef DEBUG_JUMP
    ThreadCatches (thread);
#endif
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("exception call argument must be array"),
				NewInt (0), args);
	*stack = 1;
	RETURN (Void);
    }
    if (aborting)
    {
	*stack = 1;
	RETURN (Void);
    }
    complete = True;
    argc = ArrayLimits(&args->array)[0];
    for (i = 0; i < argc; i++)
    {
	STACK_PUSH (thread->thread.continuation.stack,
		    thread->thread.continuation.value);
	thread->thread.continuation.value = ArrayValue(&args->array, i);
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

static void
ThreadUnwind (Value thread, int twixt, int catch)
{
    while (twixt--)
	thread->thread.continuation.twixts = thread->thread.continuation.twixts->continuation.twixts;
#ifdef DEBUG_JUMP
    FilePrintf (FileStdout, "     Before unwind: ");
    ThreadCatches (thread);
#endif
    while (catch--)
    {
#ifdef DEBUG_JUMP
	FilePrintf (FileStdout, "    Unwind: %A\n",
			thread->thread.continuation.catches->exception->symbol.name);
#endif
	thread->thread.continuation.catches = thread->thread.continuation.catches->continuation.catches;
    }
#ifdef DEBUG_JUMP
    FilePrintf (FileStdout, "     After unwind: ");
    ThreadCatches (thread);
#endif
}

#define ThreadBoxCheck(box,i) (BoxValueGet(box,i) == 0 ? ThreadBoxSetDefault(box,i,0) : 0)

typedef struct _TypeChain {
    struct _TypeChain	*prev;
    Type   *type;
} TypeChain;

static void
ThreadBoxSetDefault (BoxPtr box, int i, TypeChain *chain)
{
    if (BoxValueGet (box, i) == 0)
    {
	Type	    *ctype = TypeCanon (BoxType (box, i));
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
		    ThreadBoxSetDefault (box, i, &link);
	    }
	    break;
	default:
	    break;
	}
    }
}

static Value
ThreadOpArray (Value thread, Value value, int stack, Bool fetch, Bool typeCheck)
{
    Value   v;
    char    *s;
    int	    i;
    
    v = Stack(stack-1);
    switch (ValueTag(v)) {
    case rep_string:
	if (!fetch)
	{
	    RaiseStandardException (exception_invalid_binop_values, 2,
				    v, value);
	    break;
	}
	if (stack != 1)
	{
	    RaiseStandardException (exception_invalid_binop_values, 2,
				    NewInt (stack), v);
	    break;
	}
	i = IntPart (value, "Invalid string index");
	if (aborting)
	    break;
	s = StringChars (&v->string);
	if (i < 0 || StringLength (s, v->string.length) <= i)
	{
	    RaiseStandardException (exception_invalid_binop_values, 2,
				    v, value);
	    break;
	}
	value = NewInt (StringGet (s, v->string.length, i));
	break;
    case rep_array:
	if (stack != v->array.ndim)
	{
	    RaiseStandardException (exception_invalid_binop_values, 2,
				    NewInt (stack), v);
	    break;
	}
	i = ThreadArrayIndex (v, thread, stack, value, 0, True, !fetch);
	if (!aborting)
	{
	    BoxPtr  box = ArrayValueBox (&v->array, i);
	    int	    elt = ArrayValueElt (&v->array, i);
	    if (typeCheck)
		ThreadBoxCheck (box, elt);
	    if (fetch)
		value = BoxValue (box, elt);
	    else
		value = NewRef (box, elt);
	}
	break;
    case rep_hash:
	if (stack != 1)
	{
	    RaiseStandardException (exception_invalid_binop_values, 2,
				    NewInt (stack), v);
	    break;
	}
	if (fetch)
	    value = HashGet (v, value);
	else
	    value = HashRef (v, value);
	break;
    default:
	RaiseStandardException (exception_invalid_unop_value, 1, v);
	break;
    }
    return value;
}

static Value
ThreadOpDot (Value thread, Value value, Atom atom, Bool fetch)
{
    Value   v;
    
    switch (ValueTag(value)) {
    default:
	RaiseStandardException (exception_invalid_unop_value, 1, value);
	break;
    case rep_struct:
	if (fetch)
	    v = StructMemValue (value, atom);
	else
	    v = StructMemRef (value, atom);
	if (!v)
	{
	    RaiseStandardException (exception_invalid_struct_member, 2,
				    value, NewStrString (AtomName (atom)));
	    break;
	}
	value = v;
	break;
    case rep_union:
	if (fetch)
	    v = UnionValue (value, atom);
	else
	    v = UnionRef (value, atom);
	if (!v)
	{
	    if (StructMemType (value->unions.type, atom))
		RaiseStandardException (exception_invalid_struct_member, 2,
					value, NewStrString (AtomName (atom)));
	    else
		RaiseStandardException (exception_invalid_struct_member, 2,
					value, NewStrString (AtomName (atom)));
	    break;
	}
	value = v;
	break;
    }
    return value;
}

#include	<signal.h>

void
ThreadStackDump (Value thread)
{
    StackObject	    *cstack = thread->thread.continuation.stack;
    StackChunk	    *chunk;
    StackPointer    stackPointer;
    int		    i = 0;

    chunk = cstack->current;
    stackPointer = STACK_TOP(cstack);
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

#ifdef DEBUG_INST
int dump_inst = 0;
#endif

#ifdef VALIDATE_EXECUTION

extern DataType FrameType, BoxType, stackType, stackChunkType, ObjType;

static void
ObjValid(ObjPtr obj, InstPtr pc)
{
    assert(obj->data == &ObjType);
    assert(0 <= obj->used && obj->used <= obj->size);
    assert(0 <= obj->used_stat && obj->used_stat <= obj->size_stat);
    assert(obj->insts <= pc && pc < &obj->insts[obj->used]);
}

static void
FrameValid(FramePtr frame)
{
    assert (frame->data == &FrameType);

    if (frame->previous)
	assert (frame->previous->data == &FrameType);

    if (frame->staticLink)
	assert (frame->staticLink->data == &FrameType);

    assert (ValueIsFunc(frame->function));

    if (frame->frame)
	assert (frame->frame->data == &BoxType);
    if (frame->statics)
	assert (frame->statics->data == &BoxType);

    ObjValid(frame->saveObj, frame->savePc);
}

static void
StackValid(StackObject *stack)
{
    StackChunk		*chunk;
    StackElement	*stackPointer;

    assert(stack->type == &stackType);

    chunk = stack->current;
    if (chunk) {
	assert(chunk->type == &stackChunkType);
	if (chunk->previous)
	    assert (chunk->previous->type == &stackChunkType);
    }

    stackPointer = stack->stackPointer;
    assert ((stackPointer == NULL) == (chunk == NULL));
    if (stackPointer) {
	assert (&chunk->elements[0] <= stackPointer &&
		stackPointer <= &chunk->elements[STACK_ENTS_PER_CHUNK]);
    }
}

static void
ThreadValid(Value thread)
{
    Thread 	*t;
    FramePtr	frame;
    StackObject	*stack;

    assert(thread->value.type == &ThreadRep);
    t = &thread->thread;

    /* Check to make sure object is valid */
    ObjValid(t->continuation.obj,
	     t->continuation.pc);

    /* Check for valid frame */
    frame = t->continuation.frame;
    if (frame)
	FrameValid(frame);

    stack = t->continuation.stack;
    if (stack)
	StackValid(stack);
}

#define EXEC_HISTORY	256
Continuation	exec_history[EXEC_HISTORY];
int		exec_history_i;

static inline void
ExecRecord(Value thread)
{
    exec_history[exec_history_i] = thread->thread.continuation;
    exec_history_i = (exec_history_i + 1) % EXEC_HISTORY;
}
#else
#define ThreadValid(t)
#define ExecRecord(t)
#endif

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
		ThreadsSignal (NewInt (SIGINT));
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
	    if (signalChild)
	    {
		signalChild = False;
		ProcessInterrupt ();
	    }
	    if (lex && !(lex->file.flags & (FileInputBlocked|FileOutputBlocked)))
		break;
	}
	else if (thread && thread->thread.state == ThreadFinished)
	    break;
	else if (!running)
	{
	    /* when all threads are done, and all input is read, time to go */
	    if (!lex && !stopped)
		break;
	    ThreadsBlock ();
	}
	else 
	{
	    ENTER ();
	    Value	thread = running;
	    StackObject	*cstack;
	    InstPtr	inst, next;
	    FramePtr	fp;
	    int		i, j;
	    int		stack;
	    Value	value, v, w;
	    BoxPtr	box;

	    inst = thread->thread.continuation.pc;
	    value = thread->thread.continuation.value;
	    cstack = thread->thread.continuation.stack;
	    for (j = 0; j < 10; j++)
	    {
		ThreadValid(thread);
		ExecRecord(thread);
		stack = 0;
		next = inst + 1;
		complete = False;
#ifdef DEBUG_INST
		if (dump_inst)
		{
		    InstDump (inst, 1, 0, 0, 0);
		    FileFlush (FileStdout, True);
		}
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
			RaiseStandardException (exception_invalid_argument, 3,
						NewStrString ("conditional expression not bool"),
						value, Void);
			break;
		    }
		    if (!True (value))
			next = inst + inst->branch.offset;
		    break;
		case OpBranchTrue:
		    if (!ValueIsBool(value))
		    {
			RaiseStandardException (exception_invalid_argument, 3,
						NewStrString ("conditional expression not bool"),
						value, Void);
			break;
		    }
		    if (True (value))
			next = inst + inst->branch.offset;
		    break;
		case OpCase:
		    value = Equal (CStack(stack), value);
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
			RaiseStandardException (exception_invalid_argument, 3,
						NewStrString ("union switch expression not union"),
						value, Void);
			break;
		    }
		    if (value->unions.tag == inst->tagcase.tag)
		    {
			next = inst + inst->tagcase.offset;
			v = UnionValue (value, inst->tagcase.tag);
			if (!v)
			{
			    if (StructMemType (value->unions.type, inst->atom.atom))
				RaiseStandardException (exception_invalid_struct_member, 2,
							value, NewStrString (AtomName (inst->atom.atom)));
			    else
				RaiseStandardException (exception_invalid_struct_member, 2,
							value, NewStrString (AtomName (inst->atom.atom)));
			    break;
			}
			value = v;
		    }
		    break;
		case OpTagGlobal:
		    box = inst->box.box;
		    ThreadAssign (NewRef (inst->box.box, 0), value, True);
		    break;
		case OpTagLocal:
		    box = thread->thread.continuation.frame->frame;
		    i = inst->frame.element;
		    ThreadAssign (NewRef (box, i), value, True);
		    break;
		case OpReturnVoid:
		    value = Void;
		    /* fall through */
		case OpReturn:
		    if (!thread->thread.continuation.frame)
		    {
			RaiseStandardException (exception_invalid_argument, 3,
						NewStrString ("return outside of function"),
						Void, Void);
			break;
		    }
		    if (!TypeCompatibleAssign (thread->thread.continuation.frame->function->func.code->base.type,
					       value))
		    {
			RaiseStandardException (exception_invalid_argument, 3,
						NewStrString ("Incompatible type in return"),
						value, Void);
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
		    ThreadBoxCheck (inst->box.box, 0);
		    value = BoxValue (inst->box.box, 0);
		    break;
		case OpGlobalRef:
		    ThreadBoxCheck (inst->box.box, 0);
		    /* fall through... */
		case OpGlobalRefStore:
		    value = NewRef (inst->box.box, 0);
		    break;
		case OpStatic:
		case OpStaticRef:
		case OpStaticRefStore:
		    for (i = 0, fp = thread->thread.continuation.frame; i < inst->frame.staticLink; i++)
			fp = fp->staticLink;
		    box = fp->statics;
		    i = inst->frame.element;
		    if (inst->base.opCode != OpStaticRefStore)
			ThreadBoxCheck (box, i);
		    if (inst->base.opCode == OpStatic)
			value = BoxValue (box, i);
		    else
			value = NewRef (box, i);
		    break;
		case OpLocal:
		case OpLocalRef:
		case OpLocalRefStore:
		    for (i = 0, fp = thread->thread.continuation.frame; i < inst->frame.staticLink; i++)
			fp = fp->staticLink;
		    box = fp->frame;
		    i = inst->frame.element;
		    if (inst->base.opCode != OpLocalRefStore)
			ThreadBoxCheck (box, i);
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
		    value = ThreadArray (thread, inst->array.resizable,
					 stack, inst->array.type);
		    break;
		case OpBuildArrayInd:
		    value = ThreadArrayInd (thread, inst->array.resizable,
					    value, inst->array.type);
		    break;
		case OpInitArray:
		    stack = 0;
		    value = ThreadArrayInit (thread, value, inst->ainit.mode,
					     inst->ainit.dim, &stack, &next);
		    break;
		case OpBuildHash:
		    value = NewHash (False, inst->hash.type->hash.keyType,
				     inst->hash.type->hash.type);
		    break;
		case OpInitHash:
		    w = CStack (1);
		    v = CStack (0);
		    stack = 2;
		    HashSet (w, v, value);
		    value = w;
		    break;
		case OpInitHashDef:
		    v = CStack (0);
		    stack = 1;
		    HashSetDef (v, value);
		    value = v;
		    break;
		case OpBuildStruct:
		    value = NewStruct (inst->structs.structs, False);
		    break;
		case OpInitStruct:
		    w = CStack(0); stack = 1;
		    v = StructMemRef (w, inst->atom.atom);
		    if (!v)
		    {
			RaiseStandardException (exception_invalid_struct_member, 2,
						v, NewStrString (AtomName (inst->atom.atom)));
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
			RaiseStandardException (exception_invalid_struct_member, 2,
						value, NewStrString (AtomName (inst->atom.atom)));
			break;
		    }
		    w = CStack(0); stack = 1;
		    ThreadAssign (v, w, True);
		    break;
		case OpArray:
		case OpArrayRef:
		case OpArrayRefStore:
		    stack = inst->ints.value;
		    value = ThreadOpArray (thread, value, stack,
					   inst->base.opCode == OpArray,
					   inst->base.opCode != OpArrayRefStore);
		    break;
		case OpVarActual:
		    if (!ValueIsArray(value))
		    {
			RaiseStandardException (exception_invalid_unop_value, 1,
						value);
			break;
		    }
		    if (value->array.ndim != 1)
		    {
			RaiseStandardException (exception_invalid_unop_value, 1,
						value);
			break;
		    }
		    for (i = 0; i < ArrayLimits(&value->array)[0]; i++)
		    {
			STACK_PUSH (cstack, ArrayValue (&value->array, i));
			if (aborting)
			{
			    STACK_DROP (cstack, i + 1);
			    break;
			}
		    }
		    if (i != ArrayLimits(&value->array)[0])
			break;
		    complete = True;
		    value = NewInt (ArrayLimits(&value->array)[0]);
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
		    if (!ValueIsRef(value))
		    {
			RaiseStandardException (exception_invalid_unop_value, 1,
						value);
			break;
		    }
		    value = RefValueGet(value);
		    /* fall through ... */
		case OpDot:
		case OpDotRef:
		case OpDotRefStore:
		    value = ThreadOpDot (thread, value, inst->atom.atom,
					 inst->base.opCode == OpArrow || inst->base.opCode == OpDot);
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
			RaiseStandardException (exception_invalid_argument, 3,
						NewStrString ("StaticInitDone outside of function"),
						Void, Void);
			break;
		    }
		    if (aborting)
			break;
		    complete = True;
		    next = thread->thread.continuation.frame->savePc;
		    thread->thread.continuation.obj = thread->thread.continuation.frame->saveObj;
		    thread->thread.continuation.frame = thread->thread.continuation.frame->previous;
		    /* Fetch the Obj from the stack */
		    value = CStack (0); stack = 1;
		    break;
		case OpBinOp:
		    value = BinaryOperate (CStack(0), value, inst->binop.op); stack = 1;
		    break;
		case OpBinFunc:
		    value = (*inst->binfunc.func) (CStack(0), value); stack = 1;
		    break;
		case OpUnOp:
		    value = UnaryOperate (value, inst->unop.op);
		    break;
		case OpUnFunc:
		    value = (*inst->unfunc.func) (value);
		    break;
		case OpPreOp:
		    v = Dereference (value);
		    if (aborting)
			    break;
		    v = ValueIncDec (v, inst->binop.op);
		    ThreadAssign (value, v, False);
		    value = v;
		    break;
		case OpPostOp:
		    v = Dereference (value);
		    if (aborting)
			    break;
		    ThreadAssign (value, ValueIncDec (v, inst->binop.op), False);
		    value = v;
		    break;
		case OpAssign:
		    ThreadAssign (CStack(0), value, inst->assign.initialize); stack = 1;
		    break;
		case OpAssignOp:
		    v = BinaryOperate (CStack(0), value, inst->binop.op);
		    ThreadAssign (CStack(1), v, False); stack = 2;
		    value = v;
		    break;
		case OpAssignFunc:
		    v = (*inst->binfunc.func) (CStack(0), value);
		    ThreadAssign (CStack(1), v, False); stack = 2;
		    value = v;
		    break;
		case OpFork:
		    value = NewThread (thread->thread.continuation.frame, inst->obj.obj); 
		    break;
		case OpCatch:
		    if (aborting)
			break;
#ifdef DEBUG_JUMP
		    FilePrintf (FileStdout, "    Catch: %A ",
				inst->catch.exception->symbol.name);
		    ThreadCatches (thread);
#endif
		    thread->thread.continuation.catches = NewCatch (thread,
								    inst->catch.exception);
		    complete = True;
		    next = inst + inst->catch.offset;
		    break;
		case OpEndCatch:
		    if (aborting)
			break;
		    ThreadEndCatch (thread, inst->ints.value);
		    complete = True;
		    break;
		case OpRaise:
		    if (aborting)
			break;
		    value = ThreadRaise (thread, value, inst->raise.argc, inst->raise.exception, &next);
		    break;
		case OpExceptionCall:
		    ThreadExceptionCall (thread, &next, &stack);
		    break;
		case OpTwixt:
		    if (aborting)
			break;
		    thread->thread.continuation.twixts = NewTwixt (&thread->thread.continuation,
								   inst + inst->twixt.enter,
								   inst + inst->twixt.leave);
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
			if (next)
			    complete = True;
		    }
		    break;
		case OpLeaveDone:
		    if (thread->thread.jump)
		    {
			if (aborting)
			    break;
			value = JumpContinue (thread, &next);
			if (next)
			    complete = True;
		    }
		    break;
		case OpFarJump:
		    ThreadFarJump (thread, value, inst->farJump.farJump, &next);
		    break;
		case OpUnwind:
		    ThreadUnwind (thread, inst->unwind.twixt, inst->unwind.catch);
		    break;
		case OpIsType:
		    value = ValueIsType(value, inst->isType.type) ? TrueVal : FalseVal;
		    break;
		case OpHasMember:
		    if (ValueTag(value) != rep_struct)
			value = FalseVal;
		    else {
			if (StructMemType(value->structs.type, inst->atom.atom))
			    value = TrueVal;
			else
			    value = FalseVal;
		    }
		    break;
		case OpEnd:
		    SetSignalFinished ();
		    break;
		case OpDrop:
		    stack = 1;
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
		if (aborting && !complete)
		{
		    /*
		     * Check for pending execution signals
		     */
		    if (signalSuspend)
		    {
			signalSuspend = False;
			if (thread->thread.state == ThreadRunning)
			    ThreadSetState (thread, ThreadSuspended);
		    }
		    if (signalFinished)
		    {
			signalFinished = False;
			ThreadFinish (thread, False);
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
		    }
		    if (signalProfile)
			signalProfile = False;
		    break;
		}
		/* have to do this before the pc is updated */
		if (signalProfile)
		{
		    signalProfile = False;
		    ProfileInterrupt (running);
		}
		/* this instruction has been completely executed */
		thread->thread.partial = 0;
		thread->thread.continuation.value = value;
		cstack = thread->thread.continuation.stack;
		if (stack)
		    STACK_DROP (cstack, stack);
		if (inst->base.flags & InstPush)
		    STACK_PUSH (cstack, value);
		inst = next;
		thread->thread.continuation.pc = inst;
		if (thread->thread.next)
		    ThreadStepped (thread);
		ThreadValid(thread);
		if (running != thread)
		    break; 
	    }
	    EXIT ();
	}
    }
}
