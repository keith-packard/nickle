/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	"gram.h"
#include	<assert.h>

#undef DEBUG

SymbolPtr CompileNamespace (ExprPtr);

static void
ObjMark (void *object)
{
    ObjPtr  obj = object;
    InstPtr inst;
    int	    i;

    MemReference (obj->nonLocal);
    inst = ObjCode (obj, 0);
    for (i = 0; i < obj->used; i++, inst++)
    {
	MemReference (inst->base.stat);
	switch (inst->base.opCode) {
	case OpName:
	case OpNameRef:
	case OpNameRefStore:
	    MemReference (inst->var.name);
	    break;
	case OpBuildStruct:
	    MemReference (inst->structs.structs);
	    break;
	case OpBuildArray:
	    MemReference (inst->array.type);
	    break;
	case OpConst:
	    MemReference (inst->constant.constant);
	    break;
	case OpObj:
	    MemReference (inst->code.code);
	    break;
	case OpFork:
	    MemReference (inst->obj.obj);
	    break;
	case OpCatch:
	    MemReference (inst->catch.exception);
	    break;
	case OpRaise:
	    MemReference (inst->raise.exception);
	    break;
	default:
	    break;
	}
    }
}

DataType    ObjType = { ObjMark, 0 };

static ObjPtr
NewObj (int size)
{
    ENTER ();
    ObjPtr  obj;

    obj = ALLOCATE (&ObjType, sizeof (Obj) + size * sizeof (Inst));
    obj->size = size;
    obj->used = 0;
    obj->error = False;
    obj->nonLocal = 0;
    RETURN (obj);
}

#define OBJ_INCR    32

static ObjPtr
AddInst (ObjPtr obj)
{
    ENTER ();
    ObjPtr  nobj;
    
    if (obj->used == obj->size)
    {
	nobj = NewObj (obj->size + OBJ_INCR);
	memcpy (ObjCode (nobj, 0), ObjCode (obj, 0), obj->used * sizeof (Inst));
	nobj->used = obj->used;
	nobj->error = obj->error;
	nobj->nonLocal = obj->nonLocal;
	obj = nobj;
    }
    obj->used++;
    RETURN (obj);
}

#define NewInst(_i,_o)	(((_o) = AddInst(_o)), _i = ObjLast(_o))
#define BuildInst(_o,_op,_inst,_stat) \
{\
    (_o) = AddInst (_o); \
    (_inst) = ObjCode(_o, ObjLast(_o)); \
    (_inst)->base.opCode = (_op); \
    (_inst)->base.stat = (_stat); \
}
#define SetPush(_o)  ((_o)->used ? (ObjCode((_o), \
					     ObjLast(_o))->base.push = True) \
				     : 0)

/*
 * Select the correct code body depending on whether
 * we're compiling a static initializer
 */
#define CodeBody(c) ((c)->func.inStaticInit ? &(c)->func.staticInit : &(c)->func.body)

typedef enum _tail { TailNever, TailVoid, TailAlways } Tail;

ObjPtr	CompileLvalue (ObjPtr obj, ExprPtr expr, ExprPtr stat, CodePtr code, Bool createIfNecessary, Bool assign, Bool initialize);
ObjPtr	CompileBinary (ObjPtr obj, ExprPtr expr, OpCode opCode, ExprPtr stat, CodePtr code);
ObjPtr	CompileUnary (ObjPtr obj, ExprPtr expr, OpCode opCode, ExprPtr stat, CodePtr code);
ObjPtr	CompileAssign (ObjPtr obj, ExprPtr expr, OpCode opCode, ExprPtr stat, CodePtr code);
ObjPtr	CompileArrayIndex (ObjPtr obj, ExprPtr expr, ExprPtr stat, CodePtr code, int *ndimp);
ObjPtr	CompileCall (ObjPtr obj, ExprPtr expr, Tail tail, ExprPtr stat, CodePtr code);
ObjPtr	_CompileExpr (ObjPtr obj, ExprPtr expr, Bool evaluate, ExprPtr stat, CodePtr code);
void	CompilePatchLoop (ObjPtr obj, int start, int continue_offset);
ObjPtr	_CompileStat (ObjPtr obj, ExprPtr expr, Bool last, CodePtr code);
ObjPtr	CompileFunc (ObjPtr obj, CodePtr code, ExprPtr stat, CodePtr previous);
ObjPtr	CompileDecl (ObjPtr obj, ExprPtr decls, Bool evaluate, ExprPtr stat, CodePtr code);
ObjPtr	CompileFuncCode (CodePtr code, ExprPtr stat, CodePtr previous);
void	CompileError (ObjPtr obj, ExprPtr stat, char *s, ...);

/*
 * Set storage information for new symbols
 */
static void
CompileStorage (ObjPtr obj, ExprPtr stat, SymbolPtr symbol, CodePtr code)
{
    ENTER ();
    
    if (!symbol)
	obj->error = True;
    /*
     * For symbols hanging from a frame (statics, locals and args),
     * locate the frame and set their element value
     */
    else if (ClassFrame(symbol->symbol.class))
    {
	switch (symbol->symbol.class) {
	case class_static:
	    symbol->local.element = AddBoxTypes (&code->func.statics,
						 symbol->symbol.type);
	    symbol->local.staticScope = True;
	    symbol->local.code = code;
	    break;
	case class_arg:
	case class_auto:
	    symbol->local.element = AddBoxTypes (&CodeBody (code)->dynamics,
						 symbol->symbol.type);
	    symbol->local.staticScope = code->func.inStaticInit;
	    symbol->local.code = code;
	    break;
	default:
	    break;
	}
    }
    EXIT ();
}

/*
 * Make sure a symbol is valid
 */
static SymbolPtr
CompileCheckSymbol (ObjPtr obj, ExprPtr stat, ExprPtr name, CodePtr code,
		    int *depth, Bool createIfNecessary)
{
    ENTER ();
    SymbolPtr   s;
    int		d;
    CodePtr	c;

    s = name->atom.symbol;
    if (!s)
    {
        CompileError (obj, stat, "No visible symbol \"%A\" in scope%s", 
		      name->atom.atom,
		      name->atom.privateFound ? " (found non-public symbol)" : "");
	RETURN (0);
    }
    /*
     * For args and autos, make sure we're not compiling a static
     * initializer, in that case, the locals will not be in dynamic
     * namespace
     */
    d = 0;
    switch (s->symbol.class) {
    case class_static:
    case class_arg:
    case class_auto:
	/*
	 * See if the name is above a global init scope
	 */
	for (c = code; c; c = c->base.previous)
	    if (c->func.inGlobalInit)
		break;
	for (; c; c = c->base.previous)
	    if (c == s->local.code)
		break;
	if (c)
	{
	    CompileError (obj, stat, "\"%A\" not in global initializer scope",
			  name->atom.atom);
	    break;
	}

	c = s->local.code;
	if (!c)
	{
	    CompileError (obj, stat, "Class %C invalid at global scope",
			  s->symbol.class);
	    break;
	}
	/*
	 * Ensure the dynamic scope will exist
	 */
	if (c->func.inStaticInit && !s->local.staticScope)
        {
	    CompileError (obj, stat, "\"%A\" not in static initializer scope",
			  name->atom.atom);
	    break;
	}
	
	/*
	 * Compute the static link offset
	 */
	d = 0;
	for (c = code; c && c != s->local.code; c = c->base.previous)
	    d++;
	break;
    default:
	break;
    }
    *depth = d;
    RETURN (s);
}

void
CompileError (ObjPtr obj, ExprPtr stat, char *s, ...)
{
    va_list args;

    FilePrintf (FileStderr, "-> ");
    PrettyStat (FileStderr, stat, False);
    if (stat->base.file)
	FilePrintf (FileStderr, "%A:%d: ", stat->base.file, stat->base.line);
    va_start (args, s);
    FileVPrintf (FileStderr, s, args);
    va_end (args);
    FilePrintf (FileStderr, "\n");
    obj->error = True;
}

/*
 * Compile the left side of an assignment statement.
 * The result is a 'ref' left in the value register
 */
ObjPtr
CompileLvalue (ObjPtr obj, ExprPtr expr, ExprPtr stat, CodePtr code,
		Bool createIfNecessary, Bool assign, Bool initialize)
{
    ENTER ();
    InstPtr	inst;
    SymbolPtr	s;
    int		depth;
    int		ndim;
    
    switch (expr->base.tag) {
    case NAME:
	s = CompileCheckSymbol (obj, stat, expr, code,
				&depth, createIfNecessary);
	if (!s)
	{
	    expr->base.type = typesPoly;
	    break;
	}
	if (!ClassStorage (s->symbol.class) || (!initialize && s->symbol.class == class_const))
	{
	    CompileError (obj, stat, "Invalid use of %C \"%A\"",
			  s->symbol.class, expr->atom.atom);
	
	    expr->base.type = typesPoly;
	    break;
	}
	if (assign)
	{
	    BuildInst(obj, OpNameRefStore, inst, stat);
	}
	else
	{
	    BuildInst(obj, OpNameRef, inst, stat);
	}
	inst->var.name = s;
	inst->var.staticLink = depth;
	expr->base.type = s->symbol.type;
	break;
    case COLONCOLON:
	obj = CompileLvalue (obj, expr->tree.right, stat, code, False, assign, initialize);
	expr->base.type = expr->tree.right->base.type;
        break;
    case DOT:
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	expr->base.type = TypeCombineStruct (expr->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->atom.atom);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Object left of '.' is not a struct or union containing \"%A\"",
			  expr->tree.right->atom.atom);
	    expr->base.type = typesPoly;
	    break;
	}
	if (assign)
	{
	    BuildInst (obj, OpDotRefStore, inst, stat);
	}
	else
	{
	    BuildInst (obj, OpDotRef, inst, stat);
	}
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case ARROW:
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	expr->base.type = TypeCombineStruct (expr->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->atom.atom);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Object left of '->' is not a struct or union containing \"%A\"",
			  expr->tree.right->atom.atom);
	    expr->base.type = typesPoly;
	    break;
	}
	if (assign)
	{
	    BuildInst (obj, OpArrowRefStore, inst, stat);
	}
	else
	{
	    BuildInst (obj, OpArrowRef, inst, stat);
	}
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case OS:
	obj = CompileArrayIndex (obj, expr->tree.right,
				 stat, code, &ndim);
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	expr->base.type = TypeCombineArray (expr->tree.left->base.type,
					    ndim,
					    True);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible type '%T' in array operation",
			  expr->tree.left->base.type, ndim);
	    expr->base.type = typesPoly;
	    break;
	}
	if (assign)
	{
	    BuildInst (obj, OpArrayRefStore, inst, stat);
	}
	else
	{
	    BuildInst (obj, OpArrayRef, inst, stat);
	}
	inst->ints.value = ndim;
	break;
    case STAR:
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	expr->base.type = TypeCombineUnary (expr->tree.left->base.type, expr->base.tag);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible type '%T' in unary operation",
			  expr->tree.left->base.type);
	    expr->base.type = typesPoly;
	    break;
	}
	break;
    default:
	CompileError (obj, stat, "Invalid lvalue");
        expr->base.type = typesPoly;
	break;
    }
    assert (expr->base.type);
    RETURN (obj);
}

/*
 * Compile a binary operator --
 * compile the left side, push, compile the right and then
 * add the operator
 */
ObjPtr
CompileBinary (ObjPtr obj, ExprPtr expr, OpCode opCode, ExprPtr stat, CodePtr code)
{
    ENTER ();
    InstPtr inst;
    
    obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
    SetPush (obj);
    obj = _CompileExpr (obj, expr->tree.right, True, stat, code);
    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					 expr->base.tag,
					 expr->tree.right->base.type);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible types '%T', '%T' in binary operation",
		      expr->tree.left->base.type,
		      expr->tree.right->base.type);
	expr->base.type = typesPoly;
    }
    BuildInst (obj, opCode, inst, stat);
    RETURN (obj);
}

/*
 * Unaries are easy --
 * compile the operand and add the operator
 */
ObjPtr
CompileUnary (ObjPtr obj, ExprPtr expr, OpCode opCode, ExprPtr stat, CodePtr code)
{
    ENTER ();
    InstPtr inst;
    ExprPtr down;
    
    if (expr->tree.right)
	down = expr->tree.right;
    else
	down = expr->tree.left;
    obj = _CompileExpr (obj, down, True, stat, code);
    expr->base.type = TypeCombineUnary (down->base.type, expr->base.tag);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible type '%T' in unary operation",
		      down->base.type);
	expr->base.type = typesPoly;
    }
    BuildInst (obj, opCode, inst, stat);
    RETURN (obj);
}

/*
 * Assignement --
 * compile the value, build a ref for the LHS and add the operator
 */
ObjPtr
CompileAssign (ObjPtr obj, ExprPtr expr, OpCode opCode, ExprPtr stat, CodePtr code)
{
    ENTER ();
    InstPtr inst;
    
    obj = _CompileExpr (obj, expr->tree.right, True, stat, code);
    SetPush (obj);
    obj = CompileLvalue (obj, expr->tree.left, stat, code, opCode == OpAssign, 
			 opCode == OpAssign || opCode == OpInitialize, opCode == OpInitialize);
    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					 expr->base.tag,
					 expr->tree.right->base.type);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible types in assignment '%T' = '%T'",
		      expr->tree.left->base.type,
		      expr->tree.right->base.type);
	expr->base.type = typesPoly;
    }
    BuildInst (obj, opCode, inst, stat);
    RETURN (obj);
}

static ObjPtr
CompileArgs (ObjPtr obj, int *argcp, ExprPtr arg, ExprPtr stat, CodePtr code)
{
    ENTER ();
    int	argc;
    
    argc = 0;
    while (arg)
    {
	obj = _CompileExpr (obj, arg->tree.left, True, stat, code);
	SetPush (obj);
	arg = arg->tree.right;
	argc++;
    }
    *argcp = argc;
    RETURN(obj);
}

/*
 * Typecheck function object and arguments
 */
static Bool
CompileTypecheckArgs (ObjPtr	obj,
		      Types	*type,
		      ExprPtr	args,
		      int	argc,
		      ExprPtr	stat)
{
    ENTER ();
    Bool	ret = True;
    ArgType	*argt;
    ExprPtr	arg;
    Types	*func_type;
    int		i;
    
    func_type = TypeCombineFunction (type);
    if (!func_type)
    {
	CompileError (obj, stat, "Incompatible type '%T' for call",
		      type);
	EXIT ();
	return False;
    }
    
    if (func_type->base.tag == types_func)
    {
	argt = func_type->func.args;
	arg = args;
	i = 0;
	while (arg || (argt && !argt->varargs))
	{
	    if (!argt)
	    {
		CompileError (obj, stat, "Too many parameters");
		ret = False;
		break;
	    }
	    if (!arg)
	    {
		CompileError (obj, stat, "Too few parameters");
		ret = False;
		break;
	    }
	    if (!TypeCompatible (argt->type, arg->tree.left->base.type, True))
	    {
		CompileError (obj, stat, "Incompatible types '%T', '%T' argument %d",
			      argt->type, arg->tree.left->base.type, i);
		ret = False;
	    }
	    if (arg)
		arg = arg->tree.right;
	    if (!argt->varargs)
		argt = argt->next;
	}
    }
    EXIT ();
    return ret;
}


static void
MarkNonLocal (void *object)
{
    NonLocal	*nl = object;

    MemReference (nl->prev);
}

DataType    NonLocalType = { MarkNonLocal, 0 };

static NonLocal *
NewNonLocal (NonLocal *prev, NonLocalKind kind, int target)
{
    ENTER();
    NonLocal	*nl;

    nl = ALLOCATE (&NonLocalType, sizeof (NonLocal));
    nl->prev = prev;
    nl->kind = kind;
    nl->target = target;
    RETURN (nl);
}

/*
 * Compile a function call --
 * 
 * + compile the args, pushing on the stack
 * + compile the code that generates a function object
 * + Typecheck the arguments.  Must be done here so that
 *   the type of the function is available
 * + Add the OpCall
 * + Add an OpNoop in case the result must be pushed; otherwise there's
 *   no place to hang a push bit
 */
ObjPtr
CompileCall (ObjPtr obj, ExprPtr expr, Tail tail, ExprPtr stat, CodePtr code)
{
    ENTER ();
    InstPtr inst;
    int	    argc;

    obj = CompileArgs (obj, &argc, expr->tree.right, stat, code);
    obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
    if (!CompileTypecheckArgs (obj, expr->tree.left->base.type,
			       expr->tree.right, argc, stat))
    {
	expr->base.type = typesPoly;
	RETURN (obj);
    }
    expr->base.type = TypeCombineReturn (expr->tree.left->base.type);
    if (tail == TailAlways || 
	(tail == TailVoid && 
	 TypesCanon (expr->base.type) == typesPrim[type_void]))
    {
	BuildInst (obj, OpTailCall, inst, stat);
	inst->ints.value = argc;
    }
    else
    {
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = argc;
	BuildInst (obj, OpNoop, inst, stat);
    }
    RETURN (obj);
}

/*
 * Compile an exception --
 *
 * + lookup the name
 * + compile the args, pushing
 * + typecheck the args
 * + Add the OpRaise
 */
static ObjPtr
CompileRaise (ObjPtr obj, ExprPtr expr, ExprPtr stat, CodePtr code)
{
    ENTER();
    int		argc;
    ExprPtr	name;
    SymbolPtr	sym;
    InstPtr	inst;
    
    if (expr->tree.left->base.tag == COLONCOLON)
	name = expr->tree.left->tree.right;
    else
	name = expr->tree.left;

    sym = name->atom.symbol;

    if (!sym)
    {
	CompileError (obj, stat, "No exception '%A' in scope",
		      name->atom.atom);
	RETURN (obj);
    }
    if (sym->symbol.class != class_exception)
    {
	CompileError (obj, stat, "'%A' is not an exception",
		      name->atom.atom);
	RETURN (obj);
    }
    obj = CompileArgs (obj, &argc, expr->tree.right, stat, code);
    if (!CompileTypecheckArgs (obj, sym->symbol.type, expr->tree.right, argc, stat))
	RETURN(obj);
    expr->base.type = typesPoly;
    BuildInst (obj, OpRaise, inst, stat);
    inst->raise.argc = argc;
    inst->raise.exception = sym;
    RETURN (obj);
}


/*
 * Compile a  twixt --
 *
 *  twixt (enter; leave) body else _else
 *
 *  enter:
 *		enter
 *	OpEnterDone	else:
 *	OpTwixt		enter: leave:
 *		body
 *	OpTwixtDone
 *  leave:
 *		leave
 *	OpLeaveDone	done:
 *  else:
 *		_else
 *  done:
 *
 */

static ObjPtr
CompileTwixt (ObjPtr obj, ExprPtr expr, ExprPtr stat, CodePtr code)
{
    ENTER ();
    int	    enter_inst, twixt_inst, twixt_body_inst,
	    enter_done_inst, leave_done_inst;
    InstPtr inst;
    
    enter_inst = obj->used;

    /* Compile enter expression */
    if (expr->tree.left->tree.left)
	obj = _CompileExpr (obj, expr->tree.left->tree.left, True, stat, code);
    else
    {
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = One;
    }
    NewInst (enter_done_inst, obj);
    
    /* here's where the twixt instruction goes */
    NewInst (twixt_inst, obj);

    /* Compile the body */
    twixt_body_inst = obj->used;
    obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalTwixt,
				 NON_LOCAL_BREAK);
    obj = _CompileStat (obj, expr->tree.right->tree.left, False, code);
    obj->nonLocal = obj->nonLocal->prev;
    CompilePatchLoop (obj, twixt_body_inst, -1);
    BuildInst (obj, OpTwixtDone, inst, stat);
    
    /* finish the twixt instruction */
    inst = ObjCode (obj, twixt_inst);
    inst->base.opCode = OpTwixt;
    inst->base.stat = stat;
    inst->twixt.enter = enter_inst - twixt_inst;
    inst->twixt.leave = obj->used - twixt_inst;

    /* Compile leave expression */
    if (expr->tree.left->tree.right)
	obj = _CompileExpr (obj, expr->tree.left->tree.right, False, stat, code);
    NewInst (leave_done_inst, obj);

    /* finish the enter_done instruction */
    inst = ObjCode (obj, enter_done_inst);
    inst->base.opCode = OpEnterDone;
    inst->base.stat = stat;
    inst->branch.offset = obj->used - enter_done_inst;
    
    /* Compile else */
    if (expr->tree.right->tree.right)
	obj = _CompileStat (obj, expr->tree.right->tree.right, False, code);
    
    /* finish the leave_done instruction */
    inst = ObjCode (obj, leave_done_inst);
    inst->base.opCode = OpLeaveDone;
    inst->base.stat = stat;
    inst->branch.offset = obj->used - leave_done_inst;

    RETURN (obj);
}

/*
 * Return an expression that will build an
 * initializer for a fully specified composite
 * type
 */

ObjPtr
CompileArrayIndex (ObjPtr obj, ExprPtr expr, 
		   ExprPtr stat, CodePtr code, int *ndimp)
{
    ENTER ();
    int		ndim;
    
    ndim = 0;
    while (expr)
    {
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	if (!TypeCompatible (typesPrim[type_integer],
			     expr->tree.left->base.type,
			     True))
	{
	    CompileError (obj, stat, "Index type '%T' is not integer",
			  expr->tree.left->base.type);
	    break;
	}
	SetPush (obj);
	expr = expr->tree.right;
	ndim++;
    }
    *ndimp = ndim;
    RETURN (obj);
}

/*
 * Calculate the number of dimensions in an array by looking at
 * the initializers
 */
static int
CompileCountInitDimensions (ExprPtr expr)
{
    int	    ndimMax, ndim;
    
    ndimMax = 0;
    if (expr->base.tag == ARRAY)
    {
	expr = expr->tree.left;
	while (expr)
	{
	    if (expr->tree.left->base.tag != DOTS)
	    {
		ndim = CompileCountInitDimensions (expr->tree.left) + 1;
		if (ndim < 0)
		    return ndim;
		if (ndimMax && ndim != ndimMax)
		    return -1;
		if (ndim > ndimMax)
		    ndimMax = ndim;
	    }
	    expr = expr->tree.right;
	}
    }
    return ndimMax;
}

static int
CompileCountDeclDimensions (ExprPtr expr)
{
    int		ndim;

    ndim = 0;
    while (expr)
    {
	expr = expr->tree.right;
	ndim++;
    }
    return ndim;
}

static ObjPtr
CompileBuildArray (ObjPtr obj, ExprPtr expr, TypesPtr type, 
		   ExprPtr dim, int ndim, 
		   ExprPtr stat, CodePtr code)
{
    ENTER ();
    InstPtr	inst;

    while (dim)
    {
	obj = _CompileExpr (obj, dim->tree.left, True, stat, code);
	SetPush (obj);
	dim = dim->tree.right;
    }
    BuildInst (obj, OpBuildArray, inst, stat);
    inst->array.ndim = ndim;
    inst->array.type = type->array.type;
    RETURN (obj);
}

static Bool
CompileSizeDimensions (ExprPtr expr, int *dims, int ndims)
{
    int	    dim;
    
    if (expr->base.tag == ARRAY)
    {
	dim = 0;
	expr = expr->tree.left;
	while (expr)
	{
	    dim++;
	    if (expr->tree.left->base.tag == DOTS)
		return False;
	    if (ndims != 1)
		CompileSizeDimensions (expr->tree.left, dims + 1, ndims - 1);
	    expr = expr->tree.right;
	}
    }
    else
    {
	dim = 1;
	    if (expr->tree.left->base.tag == DOTS)
		return False;
	if (ndims != 1)
	    CompileSizeDimensions (expr, dims + 1, ndims - 1);
    }
    if (dim > *dims)
	*dims = dim;
    return True;
}

static ObjPtr
CompileImplicitArray (ObjPtr obj, ExprPtr array, TypesPtr type,
		      ExprPtr inits, int ndim, 
		      ExprPtr stat, CodePtr code)
{
    ENTER ();
    ExprPtr sub;
    int	    *dims;
    int	    n;
    
    dims = AllocateTemp (ndim * sizeof (int));
    if (!CompileSizeDimensions (inits, dims, ndim))
    {
	CompileError (obj, stat, "Implicit dimensioned array with variable initializers");
	RETURN (obj);
    }
    sub = 0;
    for (n = 0; n < ndim; n++)
    {
	sub = NewExprTree (COMMA,
			   NewExprConst (TEN_CONST, NewInt (*dims++)),
			   sub);
    }
    obj = CompileBuildArray (obj, array, type, sub, ndim, stat, code);
    RETURN (obj);
}

static ObjPtr
CompileArrayInit (ObjPtr obj, ExprPtr expr, Types *type, ExprPtr stat, CodePtr code);
    
static ObjPtr
CompileArrayInits (ObjPtr obj, ExprPtr expr, TypesPtr type, 
		   int ndim, int *ninits, ExprPtr stat, CodePtr code)
{
    ENTER ();
    
    if (expr->base.tag == ARRAY)
    {
	if (ndim == 0 && type->base.tag == types_array)
	{
	    obj = CompileArrayInit (obj, expr, type, stat, code);
	    expr->base.type = type;
	}
	else
	{
	    int	nsub = 0;
	    InstPtr	inst;
    
	    expr = expr->tree.left;
	    while (expr)
	    {
		if (expr->tree.left->base.tag == DOTS)
		{
		    nsub = -nsub;
		    break;
		}
		else
		{
		    obj = CompileArrayInits (obj, expr->tree.left, type, ndim-1, ninits, stat, code);
		    nsub++;
		}
		expr = expr->tree.right;
	    }
	    BuildInst (obj, OpConst, inst, stat);
	    inst->constant.constant = NewInt (nsub);
	}
	SetPush (obj);
	++(*ninits);
    }
    else
    {
	obj = _CompileExpr (obj, expr, True, stat, code);
	if (!TypeCombineBinary (type, ASSIGN, expr->base.type))
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in array initialization",
			  type, expr->base.type);
	}
	SetPush (obj);
	++(*ninits);
    }
    RETURN (obj);
}

static ObjPtr
CompileArrayInit (ObjPtr obj, ExprPtr expr, Types *type, ExprPtr stat, CodePtr code)
{
    ENTER ();
    int	    ndim;
    Types   *sub = type->array.type;
    int	    i;

    ndim = CompileCountDeclDimensions (type->array.dimensions);
    if (expr)
    {
	int ninitdim;

	if (expr->base.tag != ARRAY)
	{
	    CompileError (obj, stat, "Non array initializer");
	    RETURN (obj);
	}
	ninitdim = CompileCountInitDimensions (expr);
	if (ninitdim < 0)
	{
	    CompileError (obj, stat, "Inconsistent array initializer dimensionality");
	    RETURN (obj);
	}
	if (ndim > ninitdim ||
	    (ndim < ninitdim && sub->base.tag != types_array))
	{
	    CompileError (obj, stat, "Array dimension mismatch %d != %d\n",
			  ndim, ninitdim);
	    RETURN (obj);
	}
	i = 0;
	obj = CompileArrayInits (obj, expr, sub, ndim,
				 &i, stat, code);
    }
    if (type->array.dimensions && type->array.dimensions->tree.left)
    {
	obj = CompileBuildArray (obj, expr, type, type->array.dimensions, ndim, stat, code);
    }
    else if (expr)
    {
	obj = CompileImplicitArray (obj, expr, type, expr, 
				    ndim, stat, code);
    }
    else
    {
	CompileError (obj, stat, "Non-dimensioned array with no initializers");
    }
    if (expr)
    {
	InstPtr	inst;
	BuildInst (obj, OpInitArray, inst, stat);
	inst->ints.value = i;
    }
    RETURN (obj);
}

static ExprPtr
CompileCompositeImplicitInit (Types *type)
{
    ENTER ();
    ExprPtr	    init = 0;
    Types	    *sub;
    int		    dim;
    StructTypePtr   structs;
    StructElement   *se;
    int		    i;
    
    type = TypesCanon (type);
    
    switch (type->base.tag) {
    case types_array:
	if (type->array.dimensions && type->array.dimensions->tree.left)
	{
	    sub = type->array.type;
	    init = CompileCompositeImplicitInit (sub);
	    if (init)
	    {
		init = NewExprTree (COMMA,
				    init,
				    NewExprTree (COMMA,
						 NewExprTree (DOTS, 0, 0),
						 0));
		dim = CompileCountDeclDimensions (type->array.dimensions);
		while (--dim)
		    init = NewExprTree (OC, init, 0);
		init = NewExprTree (ARRAY, init, 0);
	    }
	    init = NewExprTree (NEW, init, 0);
	    init->base.type = type;
	}
	break;
    case types_struct:
	structs = type->structs.structs;
	se = StructTypeElements (structs);
	init = 0;
	for (i = 0; i < structs->nelements; i++)
	{
	    ExprPtr	member;
	    
	    sub = TypesCanon (se[i].type);

	    member = CompileCompositeImplicitInit (sub);
	    if (member)
	    {
		init = NewExprTree (COMMA,
				    NewExprTree (ASSIGN, 
						 NewExprAtom (se[i].name, 0, False), 
						 member),
				    init);
	    }
	}
	if (init)
	    init = NewExprTree (STRUCT, init, 0);
	init = NewExprTree (NEW, init, 0);
	init->base.type = type;
	break;
    default:
	break;
    }
    RETURN (init);
}

static Bool
CompileStructInitElementIncluded (ExprPtr expr, Atom atom)
{
    while (expr)
    {
	if (atom == expr->tree.left->tree.left->atom.atom)
	    return True;
	expr = expr->tree.right;
    }
    return False;
}

static ObjPtr
CompileStructInitUninitialized (ObjPtr obj, ExprPtr expr, StructType *structs,
				ExprPtr stat, CodePtr code)
{
    int			i;
    StructElement	*se = StructTypeElements (structs);
    InstPtr		inst;

    for (i = 0; i < structs->nelements; i++)
    {
	TypesPtr	type = TypesCanon (se[i].type);

	if (!expr || !CompileStructInitElementIncluded (expr, se[i].name))
	{
	    ExprPtr init = CompileCompositeImplicitInit (type);

	    if (init)
	    {
		SetPush (obj);
		obj = _CompileExpr (obj, init, True, stat, code);
		BuildInst (obj, OpInitStruct, inst, stat);
		inst->atom.atom = se[i].name;
	    }
	}
    }
    return obj;
}

static ObjPtr
CompileStructInit (ObjPtr obj, ExprPtr expr, Types *type,
		   ExprPtr stat, CodePtr code)
{
    ENTER ();
    StructType	*structs = type->structs.structs;
    InstPtr	inst;
    ExprPtr	init;
    Types	*mem_type;
    
    for (; expr; expr = expr->tree.right)
    {
        mem_type = StructTypes (structs, expr->tree.left->tree.left->atom.atom);
	if (!mem_type)
	{
	    CompileError (obj, stat, "Type '%T' is not a struct or union containing \"%A\"",
			  type, expr->tree.left->tree.left->atom.atom);
	    continue;
	}
	init = expr->tree.left->tree.right;
	if (init->base.tag == STRUCT)
	{
	    init = NewExprTree (NEW, init, 0);
	    init->base.type = mem_type;
	}
	
	SetPush (obj);	/* push the struct */
	obj = _CompileExpr (obj, init, True, stat, code);
	if (!TypeCombineBinary (mem_type,
				ASSIGN,
				init->base.type))
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in struct initializer",
			  mem_type, init->base.type);
	    continue;
	}
	
	BuildInst (obj, OpInitStruct, inst, stat);
	inst->atom.atom = expr->tree.left->tree.left->atom.atom;
    }
    RETURN (obj);
}

static ObjPtr
CompileCatch (ObjPtr obj, ExprPtr catches, ExprPtr body, 
	      ExprPtr stat, CodePtr code)
{
    ENTER ();
    int		catch_inst, exception_inst;
    InstPtr	inst;
    ExprPtr	catch;
    ExprPtr	name;
    SymbolPtr	exception;
    Types	*catch_type;
    
    if (catches)
    {
	catch = catches->tree.left;
	/*
	 * try a catch b
	 *
	 * CATCH b OpCall EXCEPTION a ENDCATCH
	 *   +----------------------+
	 *                    +-----------------+
	 */
	
	if (catch->code.code->base.name->base.tag == COLONCOLON)
	    name = catch->code.code->base.name->tree.right;
	else
	    name = catch->code.code->base.name;
	
	exception = name->atom.symbol;

	if (!exception)
	    RETURN(obj);
	if (exception->symbol.class != class_exception)
	{
	    CompileError (obj, stat, "Invalid use of %C \"%A\" as exception",
			  exception->symbol.class, catch->code.code->base.name->atom);
	    RETURN (obj);
	}

	catch_type = NewTypesFunc (typesPoly, catch->code.code->base.args);
	if (!TypeCompatible (exception->symbol.type, catch_type, True))
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in catch",
			  catch_type, exception->symbol.type);
	    RETURN (obj);
	}
	NewInst (catch_inst, obj);

	obj = CompileFunc (obj, catch->code.code, stat, code);
	
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = -1;
	
	NewInst (exception_inst, obj);
    
	inst = ObjCode (obj, catch_inst);
	inst->base.opCode = OpCatch;
	inst->catch.offset = obj->used - catch_inst;
	inst->catch.exception = exception;
    
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalCatch, 0);
	
	obj = CompileCatch (obj, catches->tree.right, body, stat, code);
	
	obj->nonLocal = obj->nonLocal->prev;

	BuildInst (obj, OpEndCatch, inst, stat);
	
	inst = ObjCode (obj, exception_inst);
	inst->base.opCode = OpException;
	inst->branch.offset = obj->used - exception_inst;
    }
    else
	obj = _CompileStat (obj, body, False, code);
    RETURN (obj);
}

ObjPtr
_CompileExpr (ObjPtr obj, ExprPtr expr, Bool evaluate, ExprPtr stat, CodePtr code)
{
    ENTER ();
    int	    ndim;
    int	    test_inst, middle_inst;
    InstPtr inst;
    SymbolPtr	s;
    Types	*t;
    
    switch (expr->base.tag) {
    case NAME:
	BuildInst (obj, OpName, inst, stat);
	s = CompileCheckSymbol (obj, stat, expr, code,
				&inst->var.staticLink, False);
	if (!s)
	{
	    expr->base.type = typesPoly;
	    break;
	}
	if (!ClassStorage (s->symbol.class))
	{
	    CompileError (obj, stat, "Invalid use of %C \"%A\"",
			  s->symbol.class, expr->atom.atom);
	
	    expr->base.type = typesPoly;
	    break;
	}
	inst->var.name = s;
	expr->base.type = s->symbol.type;
	break;
    case VAR:
	obj = CompileDecl (obj, expr, evaluate, stat, code);
	break;
    case NEW:
	t = TypesCanon (expr->base.type);
	switch (t ? t->base.tag : types_prim) {
	case types_struct:
	    BuildInst (obj, OpBuildStruct, inst, stat);
	    inst->structs.structs = t->structs.structs;
	    if (expr->tree.left)
	    {
		if (expr->tree.left->base.tag != STRUCT)
		{
		    CompileError (obj, stat, "Non struct initializer");
		    break;
		}
		obj = CompileStructInit (obj, 
					 expr->tree.left->tree.left, 
					 t,
					 stat, code);
	    }
	    obj = CompileStructInitUninitialized (obj, 
						  expr->tree.left ?
						  expr->tree.left->tree.left : 0,
						  t->structs.structs,
						  stat, code);
	    break;
	case types_array:
	    obj = CompileArrayInit (obj, expr->tree.left, t, stat, code);
	    expr->base.type = t;
	    break;
	default:
	    CompileError (obj, stat, 
			  "Non composite type '%T' in composite initializer", t);
	    break;
	}
	break;
    case UNION:
	if (expr->tree.right)
	    _CompileExpr (obj, expr->tree.right, True, stat, code);
	else
	{
	    BuildInst (obj, OpConst, inst, stat);
	    inst->constant.constant = Void;
	}
	SetPush (obj);
	t = TypesCanon (expr->base.type);
	if (t && t->base.tag == types_union)
	{
	    StructType	*st = t->structs.structs;
	    
	    expr->tree.left->base.type = StructTypes (st, expr->tree.left->atom.atom);
	    if (!expr->tree.left->base.type)
	    {
		CompileError (obj, stat, "Type '%T' is not a union containing \"%A\"",
			      expr->base.type,
			      expr->tree.left->atom.atom);
		break;
	    }
	    BuildInst (obj, OpBuildUnion, inst, stat);
	    inst->structs.structs = st;
	    if (expr->tree.left->base.type == typesEnum)
	    {
		if (expr->tree.right)
		{
		    CompileError (obj, stat, "enum member '%A' takes no constructor argument",
				  expr->tree.left->atom.atom);
		    break;
		}
	    }
	    else
	    {
		if (!expr->tree.right)
		{
		    CompileError (obj, stat, "union member '%A' requires constructor argument",
				  expr->tree.left->atom.atom);
		    break;
		}
		if (!TypeCombineBinary (expr->tree.left->base.type,
					ASSIGN,
					expr->tree.right->base.type))
		{
		    CompileError (obj, stat, "Incompatible types '%T', '%T' in union constructor",
				  expr->tree.left->base.type,
				  expr->tree.right->base.type);
		    break;
		}
	    }
	    BuildInst (obj, OpInitUnion, inst, stat);
	    inst->atom.atom = expr->tree.left->atom.atom;
	}
	else
	{
	    CompileError (obj, stat, "Type '%T' is not a union", expr->base.type);
	    expr->base.type = typesPoly;
	    break;
	}
	break;
    case TEN_CONST:
    case OCTAL_CONST:
    case BINARY_CONST:
    case HEX_CONST:
    case CHAR_CONST:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	/*
	 * Magic - 0 is both integer and *poly
	 */
	if (expr->constant.constant->value.tag == type_int &&
	    expr->constant.constant->ints.value == 0)
	{
	    expr->base.type = typesNil;
	}
	else
	    expr->base.type = typesPrim[type_integer];
	break;
    case FLOAT_CONST:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	expr->base.type = typesPrim[type_rational];
	break;
    case STRING_CONST:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	expr->base.type = typesPrim[type_string];
	break;
    case VOIDVAL:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	expr->base.type = typesPrim[type_void];
	break;
    case POLY_CONST:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	expr->base.type = typesPoly;    /* FIXME composite const types */
	break;
    case OS:	    
	obj = CompileArrayIndex (obj, expr->tree.right,
				 stat, code, &ndim);
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	expr->base.type = TypeCombineArray (expr->tree.left->base.type,
					    ndim,
					    False);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Type '%T' is not a %d dimensional array or string",
			  expr->tree.left->base.type, ndim);
	    expr->base.type = typesPoly;
	    break;
	}
	BuildInst (obj, OpArray, inst, stat);
	inst->ints.value = ndim;
	break;
    case OP:	    /* function call */
	obj = CompileCall (obj, expr, TailNever, stat, code);
	break;
    case COLONCOLON:
	obj = _CompileExpr (obj, expr->tree.right, evaluate, stat, code);
	expr->base.type = expr->tree.right->base.type;
        break;
    case DOT:
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	expr->base.type = TypeCombineStruct (expr->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->atom.atom);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Type '%T' is not a struct or union containing \"%A\"",
			  expr->tree.left->base.type,
			  expr->tree.right->atom.atom);
	    expr->base.type = typesPoly;
	    break;
	}
	BuildInst (obj, OpDot, inst, stat);
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case ARROW:
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	expr->base.type = TypeCombineStruct (expr->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->atom.atom);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Type '%T' is not a struct or union ref containing \"%A\"",
			  expr->tree.left->base.type,
			  expr->tree.right->atom.atom);
	    expr->base.type = typesPoly;
	    break;
	}
	BuildInst (obj, OpArrow, inst, stat);
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case FUNCTION:
	obj = CompileFunc (obj, expr->code.code, stat, code);
	expr->base.type = NewTypesFunc (expr->code.code->base.type,
					expr->code.code->base.args);
	break;
    case STAR:	    obj = CompileUnary (obj, expr, OpStar, stat, code); break;
    case AMPER:	    
	obj = CompileLvalue (obj, expr->tree.left, stat, code, False, False, False);
	expr->base.type = NewTypesRef (expr->tree.left->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Type '%T' cannot be an l-value",
			  expr->tree.left->base.type);
	    expr->base.type = typesPoly;
	    break;
	}
	break;
    case UMINUS:    obj = CompileUnary (obj, expr, OpUminus, stat, code); break;
    case LNOT:	    obj = CompileUnary (obj, expr, OpLnot, stat, code); break;
    case BANG:	    obj = CompileUnary (obj, expr, OpBang, stat, code); break;
    case FACT:	    obj = CompileUnary (obj, expr, OpFact, stat, code); break;
    case INC:	    
	if (expr->tree.left)
	{
	    obj = CompileLvalue (obj, expr->tree.left, stat, code, False, False, False);
	    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
						 ASSIGNPLUS,
						 typesPrim[type_int]);
	    BuildInst (obj, OpPreInc, inst, stat);
	}
	else
	{
	    obj = CompileLvalue (obj, expr->tree.right, stat, code, False, False, False);
	    expr->base.type = TypeCombineBinary (expr->tree.right->base.type,
						 ASSIGNPLUS,
						 typesPrim[type_int]);
	    BuildInst (obj, OpPostInc, inst, stat);
	}
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible type '%T' in ++",
			  expr->tree.left ? expr->tree.left->base.type :
			  expr->tree.right->base.type);
	    expr->base.type = typesPoly;
	    break;
	}
	break;
    case DEC:
	if (expr->tree.left)
	{
	    obj = CompileLvalue (obj, expr->tree.left, stat, code, False, False, False);
	    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
						 ASSIGNMINUS,
						 typesPrim[type_int]);
	    BuildInst (obj, OpPreDec, inst, stat);
	}
	else
	{
	    obj = CompileLvalue (obj, expr->tree.right, stat, code, False, False, False);
	    expr->base.type = TypeCombineBinary (expr->tree.right->base.type,
						 ASSIGNMINUS,
						 typesPrim[type_int]);
	    BuildInst (obj, OpPostDec, inst, stat);
	}
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible type '%T' in --",
			  expr->tree.left ? expr->tree.left->base.type :
			  expr->tree.right->base.type);
	    expr->base.type = typesPoly;
	    break;
	}
	break;
    case PLUS:	    obj = CompileBinary (obj, expr, OpPlus, stat, code); break;
    case MINUS:	    obj = CompileBinary (obj, expr, OpMinus, stat, code); break;
    case TIMES:	    obj = CompileBinary (obj, expr, OpTimes, stat, code); break;
    case DIVIDE:    obj = CompileBinary (obj, expr, OpDivide, stat, code); break;
    case DIV:	    obj = CompileBinary (obj, expr, OpDiv, stat, code); break;
    case MOD:	    obj = CompileBinary (obj, expr, OpMod, stat, code); break;
    case POW:
	obj = _CompileExpr (obj, expr->tree.right->tree.left, True, stat, code);
	SetPush (obj);
	obj = _CompileExpr (obj, expr->tree.right->tree.right, True, stat, code);
	SetPush (obj);
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	expr->base.type = TypeCombineBinary (expr->tree.right->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in binary operation",
			  expr->tree.right->tree.left->base.type,
			  expr->tree.right->tree.right->base.type);
	    expr->base.type = typesPoly;
	    break;
	}
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = 2;
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case SHIFTL:    obj = CompileBinary (obj, expr, OpShiftL, stat, code); break;
    case SHIFTR:    obj = CompileBinary (obj, expr, OpShiftR, stat, code); break;
    case QUEST:
	/*
	 * a ? b : c
	 *
	 * a QUEST b COLON c
	 *   +-------------+
	 *           +-------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	NewInst (test_inst, obj);
	obj = _CompileExpr (obj, expr->tree.right->tree.left, True, stat, code);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpQuest;
	inst->branch.offset = obj->used - test_inst;
	obj = _CompileExpr (obj, expr->tree.right->tree.right, True, stat, code);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpColon;
	inst->branch.offset = obj->used - middle_inst;
	expr->base.type = TypeCombineBinary (expr->tree.right->tree.left->base.type,
					     COLON,
					     expr->tree.right->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in ?:",
			  expr->tree.right->tree.left->base.type,
			  expr->tree.right->tree.right->base.type);
	    expr->base.type = typesPoly;
	    break;
	}
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case LXOR:	    obj = CompileBinary (obj, expr, OpLxor, stat, code); break;
    case LAND:	    obj = CompileBinary (obj, expr, OpLand, stat, code); break;
    case LOR:	    obj = CompileBinary (obj, expr, OpLor, stat, code); break;
    case AND:
	/*
	 * a && b
	 *
	 * a ANDAND b
	 *   +--------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	NewInst (test_inst, obj);
	obj = _CompileExpr (obj, expr->tree.right, True, stat, code);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpAnd;
	inst->base.stat = stat;
	inst->branch.offset = obj->used - test_inst;
	expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					     COLON,
					     expr->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in &&",
			  expr->tree.left->base.type,
			  expr->tree.right->base.type);
	    expr->base.type = typesPoly;
	    break;
	}
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case OR:
	/*
	 * a || b
	 *
	 * a OROR b
	 *   +--------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	NewInst (test_inst, obj);
	obj = _CompileExpr (obj, expr->tree.right, True, stat, code);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpOr;
	inst->base.stat = stat;
	inst->branch.offset = obj->used - test_inst;
	expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					     COLON,
					     expr->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in ||",
			  expr->tree.left->base.type,
			  expr->tree.right->base.type);
	    expr->base.type = typesPoly;
	    break;
	}
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case ASSIGN:	obj = CompileAssign (obj, expr, OpAssign, stat, code); break;
    case ASSIGNPLUS:	obj = CompileAssign (obj, expr, OpAssignPlus, stat, code); break;
    case ASSIGNMINUS:	obj = CompileAssign (obj, expr, OpAssignMinus, stat, code); break;
    case ASSIGNTIMES:	obj = CompileAssign (obj, expr, OpAssignTimes, stat, code); break;
    case ASSIGNDIVIDE:	obj = CompileAssign (obj, expr, OpAssignDivide, stat, code); break;
    case ASSIGNDIV:	obj = CompileAssign (obj, expr, OpAssignDiv, stat, code); break;
    case ASSIGNMOD:	obj = CompileAssign (obj, expr, OpAssignMod, stat, code); break;
    case ASSIGNPOW:
	obj = _CompileExpr (obj, expr->tree.right->tree.right, True, stat, code);
	SetPush (obj);
	obj = CompileLvalue (obj, expr->tree.right->tree.left, stat, code, False, False, False);
	SetPush (obj);
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	expr->base.type = TypeCombineBinary (expr->tree.right->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in assignment",
			  expr->tree.right->tree.left->base.type,
			  expr->tree.right->tree.right->base.type);
	    expr->base.type = typesPoly;
	    break;
	}
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = 2;
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case ASSIGNSHIFTL:	obj = CompileAssign (obj, expr, OpAssignShiftL, stat, code); break;
    case ASSIGNSHIFTR:	obj = CompileAssign (obj, expr, OpAssignShiftR, stat, code); break;
    case ASSIGNLXOR:	obj = CompileAssign (obj, expr, OpAssignLxor, stat, code); break;
    case ASSIGNLAND:	obj = CompileAssign (obj, expr, OpAssignLand, stat, code); break;
    case ASSIGNLOR:	obj = CompileAssign (obj, expr, OpAssignLor, stat, code); break;
    case EQ:	    obj = CompileBinary (obj, expr, OpEq, stat, code); break;
    case NE:	    obj = CompileBinary (obj, expr, OpNe, stat, code); break;
    case LT:	    obj = CompileBinary (obj, expr, OpLt, stat, code); break;
    case GT:	    obj = CompileBinary (obj, expr, OpGt, stat, code); break;
    case LE:	    obj = CompileBinary (obj, expr, OpLe, stat, code); break;
    case GE:	    obj = CompileBinary (obj, expr, OpGe, stat, code); break;
    case COMMA:	    
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	expr->base.type = expr->tree.left->base.type;
	if (expr->tree.right)
	{
	    obj = _CompileExpr (obj, expr->tree.right, True, stat, code);
	    expr->base.type = expr->tree.right->base.type;
	}
	break;
    case FORK:
	BuildInst (obj, OpFork, inst, stat);
	inst->obj.obj = CompileExpr (expr->tree.right, code);
	expr->base.type = typesPrim[type_thread];
	break;
    case THREAD:
	obj = CompileCall (obj, NewExprTree (OP,
					     expr->tree.right,
					     NewExprTree (COMMA, 
							  expr->tree.left,
							  (Expr *) 0)),
			   TailNever,
			   stat, code);
	expr->base.type = typesPrim[type_thread];
	break;
    case DOLLAR:
	/* reposition statement reference so top-level errors are nicer*/
	obj = _CompileExpr (obj, expr->tree.left, True, expr, code);
	expr->base.type = expr->tree.left->base.type;
	break;
    }
    assert (!evaluate || expr->base.type);
    RETURN (obj);
}

void
CompilePatchLoop (ObjPtr obj, int start, int continue_offset)
{
    int	    break_offset;
    InstPtr inst;

    break_offset = obj->used;
    while (start < obj->used)
    {
	inst = ObjCode (obj, start);
	switch (inst->base.opCode) {
	case OpBreak:
	    if (inst->branch.offset == 0)
		inst->branch.offset = break_offset - start;
	    break;
	case OpContinue:
	    if (inst->branch.offset == 0 && continue_offset >= 0)
		inst->branch.offset = continue_offset - start;
	    break;
	default:
	    break;
	}
	++start;
    }
}

static ObjPtr
CompileNonLocal (ObjPtr obj, int target, ExprPtr expr)
{
    ENTER ();
    NonLocal	*nl;
    int		twixt = 0, catch = 0;

    for (nl = obj->nonLocal; nl; nl = nl->prev)
    {
	if (nl->target & target)
	    break;
	switch (nl->kind) {
	case NonLocalTwixt:
	    twixt++;
	    break;
	case NonLocalCatch:
	    catch++;
	    break;
	case NonLocalControl:
	    break;
	}
    }
    if (!nl) 
    {
	switch (target) {
	case NON_LOCAL_BREAK:
	    CompileError (obj, expr, "break not in loop/switch/twixt");
	    break;
	case NON_LOCAL_CONTINUE:
	    CompileError (obj, expr, "continue not in loop");
	    break;
	}
    }
    if (twixt || catch)
    {
	InstPtr	inst;
	BuildInst (obj, OpUnwind, inst, expr);
	inst->base.stat = expr;
	inst->unwind.twixt = twixt;
	inst->unwind.catch = catch;
    }
    RETURN (obj);
}

ObjPtr
_CompileStat (ObjPtr obj, ExprPtr expr, Bool last, CodePtr code)
{
    ENTER ();
    int		top_inst, continue_inst, test_inst, middle_inst, bottom_inst;
    ExprPtr	c;
    int		ncase, *case_inst, icase;
    Bool	has_default;
    InstPtr	inst;
    
    switch (expr->base.tag) {
    case IF:
	/*
	 * if (a) b
	 *
	 * a IF b
	 *   +----+
	 */
	obj = _CompileExpr (obj, expr->tree.left, True, expr, code);
	NewInst (test_inst, obj);
	obj = _CompileStat (obj, expr->tree.right, last, code);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpIf;
	inst->base.stat = expr;
	inst->branch.offset = obj->used - test_inst;
	break;
    case ELSE:
	/*
	 * if (a) b else c
	 *
	 * a IF b ELSE c
	 *   +---------+
	 *        +------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, True, expr, code);
	NewInst (test_inst, obj);
	obj = _CompileStat (obj, expr->tree.right->tree.left, last, code);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpIf;
	inst->base.stat = expr;
	inst->branch.offset = obj->used - test_inst;
	obj = _CompileStat (obj, expr->tree.right->tree.right, last, code);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpElse;
	inst->base.stat = expr;
	inst->branch.offset = obj->used - middle_inst;
	break;
    case WHILE:
	/*
	 * while (a) b
	 *
	 * a WHILE b ENDWHILE
	 *   +----------------+
	 * +---------+
	 */
	continue_inst = obj->used;
	obj = _CompileExpr (obj, expr->tree.left, True, expr, code);
	NewInst (test_inst, obj);
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalControl,
				     NON_LOCAL_BREAK|NON_LOCAL_CONTINUE);
	obj = _CompileStat (obj, expr->tree.right, False, code);
	obj->nonLocal = obj->nonLocal->prev;
	NewInst (bottom_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpWhile;
	inst->base.stat = expr;
	inst->branch.offset = obj->used - test_inst;
	inst = ObjCode (obj, bottom_inst);
	inst->base.opCode = OpEndWhile;
	inst->base.stat = expr;
	inst->branch.offset = continue_inst - bottom_inst;
	CompilePatchLoop (obj, continue_inst, continue_inst);
	break;
    case DO:
	/*
	 * do a while (b);
	 *
	 * a b DO
	 * +---+
	 */
	continue_inst = obj->used;
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalControl,
				     NON_LOCAL_BREAK|NON_LOCAL_CONTINUE);
	obj = _CompileStat (obj, expr->tree.left, False, code);
	obj->nonLocal = obj->nonLocal->prev;
	obj = _CompileExpr (obj, expr->tree.right, True, expr, code);
	NewInst (test_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpDo;
	inst->base.stat = expr;
	inst->branch.offset = continue_inst - test_inst;
	CompilePatchLoop (obj, continue_inst, continue_inst);
	break;
    case FOR:
	/*
	 * for (a; b; c) d
	 *
	 * a b FOR d c ENDFOR
	 *     +--------------+
	 *   +---------+
	 */
	if (expr->tree.left->tree.left)
	    obj = _CompileExpr (obj, expr->tree.left->tree.left, False, expr, code);
	top_inst = obj->used;
	test_inst = -1;
	if (expr->tree.left->tree.right)
	{
	    obj = _CompileExpr (obj, expr->tree.left->tree.right, True, expr, code);
	    NewInst (test_inst, obj);
	}
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalControl,
				     NON_LOCAL_BREAK|NON_LOCAL_CONTINUE);
	obj = _CompileStat (obj, expr->tree.right->tree.right, False, code);
	obj->nonLocal = obj->nonLocal->prev;
	continue_inst = obj->used;
	if (expr->tree.right->tree.left)
	    obj = _CompileExpr (obj, expr->tree.right->tree.left, False, expr, code);
	NewInst (bottom_inst, obj);
	if (test_inst != -1)
	{
	    inst = ObjCode (obj, test_inst);
	    inst->base.opCode = OpFor;
	    inst->base.stat = expr;
	    inst->branch.offset = obj->used - test_inst;
	}
	inst = ObjCode (obj, bottom_inst);
	inst->base.opCode = OpEndFor;
	inst->base.stat = expr;
	inst->branch.offset = top_inst - bottom_inst;
	CompilePatchLoop (obj, top_inst, continue_inst);
	break;
    case SWITCH:
    case UNION:
	/*
	 * switch (a) { case b: c; case d: e; }
	 *
	 *  a b CASE d CASE DEFAULT c e
	 *       +------------------+
	 *              +-------------+
	 *                     +--------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, True, expr, code);
	if (expr->base.tag == SWITCH)
	    SetPush (obj);
	c = expr->tree.right;
	has_default = False;
	ncase = 0;
	while (c)
	{
	    if (!c->tree.left->tree.left)
		has_default = True;
	    else
		ncase++;
	    c = c->tree.right;
	}
	case_inst = AllocateTemp (ncase * sizeof (int));
	/*
	 * Compile the comparisons
	 */
	c = expr->tree.right;
	icase = 0;
	while (c)
	{
	    if (c->tree.left->tree.left)
	    {
		if (expr->base.tag == SWITCH)
		    obj = _CompileExpr (obj, c->tree.left->tree.left, True, c, code);
		NewInst (case_inst[icase], obj);
		icase++;
	    }
	    c = c->tree.right;
	}
	/* add default case at the bottom */
    	NewInst (test_inst, obj);
	top_inst = obj->used;
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalControl,
				     NON_LOCAL_BREAK);
	/*
	 * Compile the statements
	 */
	c = expr->tree.right;
	icase = 0;
	while (c)
	{
	    ExprPtr s = c->tree.left->tree.right;

	    /*
	     * Patch the branch
	     */
	    if (c->tree.left->tree.left)
	    {
		inst = ObjCode (obj, case_inst[icase]);
		if (expr->base.tag == SWITCH)
		{
		    inst->base.opCode = OpCase;
		    inst->branch.offset = obj->used - case_inst[icase];
		}
		else
		{
		    inst->base.opCode = OpTagCase;
		    inst->tagcase.offset = obj->used - case_inst[icase];
		    inst->tagcase.tag = c->tree.left->tree.left->atom.atom;
		}
		inst->base.stat = expr;
		icase++;
	    }
	    else
	    {
		inst = ObjCode (obj, test_inst);
		if (expr->base.tag == SWITCH)
		    inst->base.opCode = OpDefault;
		else
		    inst->base.opCode = OpTagDefault;
		inst->base.stat = expr;
		inst->branch.offset = obj->used - test_inst;
	    }
	    while (s->tree.left)
	    {
		obj = _CompileStat (obj, s->tree.left, False, code);
		s = s->tree.right;
	    }
	    c = c->tree.right;
	}
	obj->nonLocal = obj->nonLocal->prev;
	if (!has_default)
	{
	    inst = ObjCode (obj, test_inst);
	    if (expr->base.tag == SWITCH)
		inst->base.opCode = OpDefault;
	    else
		inst->base.opCode = OpTagDefault;
	    inst->base.stat = expr;
	    inst->branch.offset = obj->used - test_inst;
	}
	CompilePatchLoop (obj, top_inst, -1);
	break;
    case VAR:
	obj = CompileDecl (obj, expr, False, expr, code);
	break;
    case TYPEDEF:
	break;
    case OC:
	while (expr->tree.left)
	{
	    obj = _CompileStat (obj, expr->tree.left, last && !expr->tree.right->tree.left, code);
	    expr = expr->tree.right;
	}
	break;
    case BREAK:
	obj = CompileNonLocal (obj, NON_LOCAL_BREAK, expr);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpBreak;
	inst->base.stat = expr;
	inst->branch.offset = 0;    /* filled in by PatchLoop */
	break;
    case CONTINUE:
	obj = CompileNonLocal (obj, NON_LOCAL_CONTINUE, expr);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpContinue;
	inst->base.stat = expr;
	inst->branch.offset = 0;    /* filled in by PatchLoop */
	break;
    case RETURNTOK:
	if (!code)
	{
	    CompileError (obj, expr, "return not in function");
	    break;
	}
	if (expr->tree.right)
	{
	    if (expr->tree.right->base.tag == OP)
	    {
		obj = CompileCall (obj, expr->tree.right, TailAlways, expr, code);
	    }
	    else
	    {
		obj = _CompileExpr (obj, expr->tree.right, True, expr, code);
		obj = CompileNonLocal (obj, NON_LOCAL_RETURN, expr);
		BuildInst (obj, OpReturn, inst, expr);
		inst->base.stat = expr;
	    }
	    expr->base.type = expr->tree.right->base.type;
	}
	else
	{
	    obj = CompileNonLocal (obj, NON_LOCAL_RETURN, expr);
	    BuildInst (obj, OpReturnVoid, inst, expr);
	    inst->base.stat = expr;
	    expr->base.type = typesPrim[type_void];
	}
	if (!TypeCombineBinary (code->base.type, ASSIGN, expr->base.type))
	{
	    CompileError (obj, expr, "Incompatible types '%T', '%T' in return",
			  code->base.type, expr->base.type);
	    break;
	}
	break;
    case EXPR:
	if (last && expr->tree.left->base.tag == OP)
	    obj = CompileCall (obj, expr->tree.left, TailVoid, expr, code);
	else
	    obj = _CompileExpr (obj, expr->tree.left, False, expr, code);
	break;
    case SEMI:
	break;
    case FUNCTION:
	obj = CompileDecl (obj, expr->tree.left, False, expr, code); 
	obj = CompileAssign (obj, expr->tree.right, OpInitialize, expr, code);
	break;
    case NAMESPACE:
	obj = _CompileStat (obj, expr->tree.right, last, code);
	break;
    case IMPORT:
	break;
    case CATCH:
	obj = CompileCatch (obj, expr->tree.right, expr->tree.left, expr, code);
	break;
    case RAISE:
	obj = CompileRaise (obj, expr, expr, code);
	break;
    case TWIXT:
	obj = CompileTwixt (obj, expr, expr, code);
	break;
    }
    RETURN (obj);
}

static Bool
CompileIsBranch (InstPtr inst)
{
    switch (inst->base.opCode) {
    case OpIf:
    case OpElse:
    case OpWhile:
    case OpEndWhile:
    case OpDo:
    case OpFor:
    case OpEndFor:
    case OpCase:
    case OpTagCase:
    case OpDefault:
    case OpTagDefault:
    case OpBreak:
    case OpContinue:
    case OpQuest:
    case OpColon:
    case OpAnd:
    case OpOr:
    case OpException:
    case OpLeaveDone:
    case OpCatch:
	return True;
    default:
	return False;
    }
}

static Bool
CompileOpIsReturn (OpCode op)
{
    return (op == OpReturn || op == OpReturnVoid || op == OpTailCall);
}

ObjPtr
CompileFuncCode (CodePtr code, ExprPtr stat, CodePtr previous)
{
    ENTER ();
    ObjPtr  obj;
    InstPtr inst;
    int	    i;
    Bool    needReturn;
    
    code->base.previous = previous;
    obj = NewObj (OBJ_INCR);
    obj = _CompileStat (obj, code->func.code, True, code);
    needReturn = False;
    if (!obj->used || !CompileOpIsReturn (ObjCode (obj, ObjLast(obj))->base.opCode))
	needReturn = True;
    else
    {
	for (i = 0; i < obj->used; i++)
	{
	    inst = ObjCode (obj, i);
	    if (CompileIsBranch (inst))
		if (i + inst->branch.offset == obj->used)
		{
		    needReturn = True;
		    break;
		}
	}
    }
    if (needReturn)
	BuildInst (obj, OpReturnVoid, inst, stat);
#ifdef DEBUG
    ObjDump (obj, 0);
#endif
    RETURN (obj);
}

ObjPtr
CompileFunc (ObjPtr obj, CodePtr code, ExprPtr stat, CodePtr previous)
{
    ENTER ();
    InstPtr	    inst;
    ArgType	    *args;
    ObjPtr	    staticInit;

    for (args = code->base.args; args; args = args->next)
    {
	CompileStorage (obj, stat, args->symbol, code);
	if (!args->varargs)
	    code->base.argc++;
    }
    code->func.body.obj = CompileFuncCode (code, stat, previous);
    obj->error |= code->func.body.obj->error;
    BuildInst (obj, OpObj, inst, stat);
    inst->code.code = code;
    if ((staticInit = code->func.staticInit.obj))
    {
	SetPush (obj);
	BuildInst (staticInit, OpStaticDone, inst, stat);
	BuildInst (staticInit, OpEnd, inst, stat);
#ifdef DEBUG
	ObjDump (staticInit, 1);
#endif
	code->func.staticInit.obj = staticInit;
	BuildInst (obj, OpStaticInit, inst, stat);
	BuildInst (obj, OpNoop, inst, stat);
	obj->error |= staticInit->error;
    }
    RETURN (obj);
}

/*
 * Compile a declaration expression.  Allocate storage for the symbol,
 * Typecheck and compile initializers, make sure a needed value
 * is left in the accumulator
 */

ObjPtr
CompileDecl (ObjPtr obj, ExprPtr decls, 
	     Bool evaluate, ExprPtr stat, CodePtr code)
{
    ENTER ();
    SymbolPtr	    s = 0;
    DeclListPtr	    decl;
    Class	    class;
    Publish	    publish;
    CodePtr	    code_compile = 0;
    ObjPtr	    *initObj;
    
    class = decls->decl.class;
    if (class == class_undef)
	class = code ? class_auto : class_global;
    publish = decls->decl.publish;
    switch (class) {
    case class_global:
    case class_const:
	/*
	 * Globals are compiled in the static initializer for
	 * the outermost enclosing function.
	 */
	code_compile = code;
	while (code_compile && code_compile->base.previous)
	    code_compile = code_compile->base.previous;
	break;
    case class_static:
	/*
	 * Statics are compiled in the static initializer for
	 * the nearest enclosing function
	 */
	code_compile = code;
	break;
    case class_auto:
    case class_arg:
	/*
	 * Autos are compiled where they lie; just make sure a function
	 * exists somewhere to hang them from
	 */
	break;
    default:
	break;
    }
    if (ClassFrame (class) && !code)
    {
	CompileError (obj, decls, "Invalid storage class %C", class);
	decls->base.type = typesPoly;
	RETURN (obj);
    }
    for (decl = decls->decl.decl; decl; decl = decl->next) {
	ExprPtr	init;
        CompileStorage (obj, decls, decl->symbol, code);
	s = decl->symbol;
	/*
	 * Automatically build initializers for composite types
	 * which fully specify the storage
	 */
	init = decl->init;
	if (!init && s)
	    init = CompileCompositeImplicitInit (s->symbol.type);
	if (init)
	{
	    InstPtr inst;
	    ExprPtr lvalue;

	    /*
	     * Compile the initializer value
	     */
	    initObj = &obj;
	    if (code_compile)
	    {
		if (!code_compile->func.staticInit.obj)
		    code_compile->func.staticInit.obj = NewObj (OBJ_INCR);
		initObj = &code_compile->func.staticInit.obj;
		code_compile->func.inStaticInit = True;
		if (code != code_compile)
		    code->func.inGlobalInit = True;
	    }
	    *initObj = _CompileExpr (*initObj, init, 
				     True, stat, code);
	    if (code_compile)
	    {
		code_compile->func.inStaticInit = False;
		code->func.inGlobalInit = False;
	    }
	    SetPush (*initObj);
	    
	    /*
	     * Assign it
	     */
	    lvalue = NewExprAtom (decl->name, decl->symbol, False);
	    *initObj = CompileLvalue (*initObj, lvalue,
				       decls, code, False, True, True);
	    if (!TypeCombineBinary (lvalue->base.type,
				    ASSIGN,
				    init->base.type))
	    {
		CompileError (obj, decls, "Incompatible types '%T', '%T' in initialization",
			      lvalue->base.type,
			      init->base.type);
	    }
	    BuildInst (*initObj, OpInitialize, inst, decls);
	}
    }
    if (evaluate)
    {
	if (s)
	{
	    InstPtr	inst;
	    BuildInst (obj, OpName, inst, stat);
	    inst->var.name = s;
	    decls->base.type = s->symbol.type;
	}
	else
	    decls->base.type = typesPoly;
    }
    RETURN (obj);
}

ObjPtr
CompileStat (ExprPtr expr, CodePtr code)
{
    ENTER ();
    ObjPtr  obj;
    InstPtr inst;

    obj = NewObj (OBJ_INCR);
    obj = _CompileStat (obj, expr, False, code);
    BuildInst (obj, OpEnd, inst, expr);
#ifdef DEBUG
    ObjDump (obj, 0);
#endif
    RETURN (obj);
}

ObjPtr
CompileExpr (ExprPtr expr, CodePtr code)
{
    ENTER ();
    ObjPtr  obj;
    InstPtr inst;
    ExprPtr stat;

    stat = NewExprTree (EXPR, expr, 0);
    obj = NewObj (OBJ_INCR);
    obj = _CompileExpr (obj, expr, True, stat, code);
    BuildInst (obj, OpEnd, inst, stat);
#ifdef DEBUG
    ObjDump (obj, 0);
#endif
    RETURN (obj);
}

char *OpNames[] = {
    "Noop",
    /*
     * Statement op codes
     */
    "If",
    "Else",
    "While",
    "EndWhile",
    "Do",
    "For",
    "EndFor",
    "Case",
    "TagCase",
    "Default",
    "TagDefault",
    "Break",
    "Continue",
    "Return",
    "ReturnVoid",
    "Function",
    "Fork",
    "Catch",
    "Exception",
    "EndCatch",
    "Raise",
    "OpTwixt",
    "OpTwixtDone",
    "OpEnterDone",
    "OpLeaveDone",
    "OpUnwind",
    /*
     * Expr op codes
     */
    "Name",
    "NameRef",
    "NameRefStore",
    "Const",
    "BuildArray",
    "InitArray",
    "BuildStruct",
    "InitStruct",
    "BuildUnion",
    "InitUnion",
    "Array",
    "ArrayRef",
    "ArrayRefStore",
    "Call",
    "TailCall",
    "Dot",
    "DotRef",
    "DotRefStore",
    "Arrow",
    "ArrowRef",
    "ArrowRefStore",
    "Obj",
    "StaticInit",
    "StaticDone",
    "Star",
    "Uminus",
    "Lnot",
    "Bang",
    "Fact",
    "PreInc",
    "PostInc",
    "PreDec",
    "PostDec",
    "Plus",
    "Minus",
    "Times",
    "Divide",
    "Div",
    "Mod",
    "Pow",
    "ShiftL",
    "ShiftR",
    "Quest",
    "Colon",
    "Lxor",
    "Land",
    "Lor",
    "And",
    "Or",
    "Assign",
    "AssignPlus",
    "AssignMinus",
    "AssignTimes",
    "AssignDivide",
    "AssignDiv",
    "AssignMod",
    "AssignPow",
    "AssignShiftL",
    "AssignShiftR",
    "AssignLxor",
    "AssignLand",
    "AssignLor",
    "Initialize",
    "Eq",
    "Ne",
    "Lt",
    "Gt",
    "Le",
    "Ge",
    "End",
};

static void
ObjIndent (int indent)
{
    int	j;
    for (j = 0; j < indent; j++)
        FilePrintf (FileStdout, "    ");
}

void
InstDump (InstPtr inst, int indent, int i, int *branch, int maxbranch)
{
    int	    j;
    
    ObjIndent (indent);
    FilePrintf (FileStdout, "%s%s %c ",
		OpNames[inst->base.opCode],
		"             " + strlen(OpNames[inst->base.opCode]),
		inst->base.push ? '^' : ' ');
    switch (inst->base.opCode) {
    case OpTagCase:
	FilePrintf (FileStdout, "(%T)", inst->tagcase.tag);
	goto branch;
    case OpCatch:
	FilePrintf (FileStdout, "\"%A\" ", 
		    inst->catch.exception->symbol.name);
	goto branch;
    case OpIf:
    case OpElse:
    case OpWhile:
    case OpEndWhile:
    case OpDo:
    case OpFor:
    case OpEndFor:
    case OpCase:
    case OpDefault:
    case OpTagDefault:
    case OpBreak:
    case OpContinue:
    case OpQuest:
    case OpColon:
    case OpAnd:
    case OpOr:
    case OpException:
    case OpLeaveDone:
    branch:
	if (branch)
	{
	    j = i + inst->branch.offset;
	    if (0 <= j && j < maxbranch)
		FilePrintf (FileStdout, "branch L%d", branch[j]);
	    else
		FilePrintf (FileStdout, "Broken branch %d", inst->branch.offset);
	}
	else
		FilePrintf (FileStdout, "branch %d", inst->branch.offset);
	break;
    case OpReturn:
    case OpReturnVoid:
	break;
    case OpFork:
	FilePrintf (FileStdout, "\n");
	ObjDump (inst->obj.obj, indent+1);
	break;
    case OpEndCatch:
	break;
    case OpRaise:
	FilePrintf (FileStdout, "%A", inst->raise.exception->symbol.name);
	FilePrintf (FileStdout, " argc %d", inst->raise.argc);
	break;
    case OpTwixt:
	FilePrintf (FileStdout, "enter %d leave %d",
		    inst->twixt.enter, inst->twixt.leave);
	break;
    case OpUnwind:
	FilePrintf (FileStdout, "twixt %d catch %d",
		    inst->unwind.twixt, inst->unwind.catch);
	break;
    case OpName:
    case OpNameRef:
    case OpNameRefStore:
	if (inst->var.name)
	{
	    SymbolPtr	s = inst->var.name;
	    FilePrintf (FileStdout, "%C %A", s->symbol.class, s->symbol.name);
	    switch (s->symbol.class) {
	    case class_arg:
	    case class_static:
	    case class_auto:
		FilePrintf (FileStdout, " (link %d elt %d)",
			    inst->var.staticLink,
			    s->local.element);
		break;
	    default:
		break;
	    }
	}
	else
	    FilePrintf (FileStdout, "Broken name");
	break;
    case OpConst:
	FilePrintf (FileStdout, "%v", inst->constant.constant);
	break;
    case OpCall:
    case OpTailCall:
	FilePrintf (FileStdout, "%d args", inst->ints.value);
	break;
    case OpBuildArray:
	FilePrintf (FileStdout, "%d dims", inst->ints.value);
	break;
    case OpInitArray:
	FilePrintf (FileStdout, "%d values", inst->ints.value);
	break;
    case OpDot:
    case OpDotRef:
    case OpDotRefStore:
    case OpArrow:
    case OpArrowRef:
    case OpArrowRefStore:
    case OpInitStruct:
    case OpInitUnion:
	FilePrintf (FileStdout, "%s", AtomName (inst->atom.atom));
	break;
    case OpObj:
	FilePrintf (FileStdout, "\n");
	if (inst->code.code->func.staticInit.obj)
	{
	    ObjIndent (indent);
	    FilePrintf (FileStdout, "Static initializer:\n");
	    ObjDump (inst->code.code->func.staticInit.obj, indent+1);
	    ObjIndent (indent);
	    FilePrintf (FileStdout, "Function body:\n");
	}
	ObjDump (inst->code.code->func.body.obj, indent+1);
	break;
    default:
	break;
    }
    FilePrintf (FileStdout, "\n");
}

void
ObjDump (ObjPtr obj, int indent)
{
    int	    i, j;
    InstPtr inst;
    ExprPtr stat;
    int	    *branch;
    int	    b;

    branch = AllocateTemp (obj->used * sizeof (int));
    
    ObjIndent (indent);
    FilePrintf (FileStdout, "%d instructions (0x%x)\n",
		obj->used, ObjCode(obj,0));
    b = 0;
    for (i = 0; i < obj->used; i++)
    {
	inst = ObjCode(obj, i);
	if (CompileIsBranch (inst))
	{
	    j = i + inst->branch.offset;
	    if (0 <= j && j < obj->used)
		if (!branch[j])
		    branch[j] = ++b;
	}
    }
    b = 0;
    stat = 0;
    for (i = 0; i < obj->used; i++)
    {
	inst = ObjCode(obj, i);
	if (inst->base.stat != stat)
	{
	    FilePrintf (FileStdout, "                                     ");
	    PrettyStat (FileStdout, inst->base.stat, False);
	    stat = inst->base.stat;
	}
	if (branch[i])
	    FilePrintf (FileStdout, "L%d:\n", branch[i]);
	InstDump (inst, indent, i, branch, obj->used);
    }
}

