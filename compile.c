/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"nickle.h"
#include	"gram.h"

#undef DEBUG

SymbolPtr CompileNamespace (ExprPtr, NamespacePtr);

static void
ObjMark (void *object)
{
    ObjPtr  obj = object;
    InstPtr inst;
    int	    i;

    inst = ObjCode (obj, 0);
    for (i = 0; i < obj->used; i++, inst++)
    {
	MemReference (inst->base.stat);
	switch (inst->base.opCode) {
	case OpName:
	case OpNameRef:
	    MemReference (inst->var.name);
	    break;
	case OpBuildStruct:
	    MemReference (inst->structs.structs);
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
    obj->errors = 0;
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
	nobj->errors = obj->errors;
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

ObjPtr	CompileLvalue (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, ExprPtr stat, Bool createIfNecessary);
ObjPtr	CompileBinary (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, OpCode opCode, ExprPtr stat);
ObjPtr	CompileUnary (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, OpCode opCode, ExprPtr stat);
ObjPtr	CompileAssign (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, OpCode opCode, ExprPtr stat);
ObjPtr	CompileCall (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, ExprPtr stat);
ObjPtr	CompileArray (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, OpCode opCode, ExprPtr stat);
ObjPtr	_CompileExpr (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, ExprPtr stat);
void	CompilePatchLoop (ObjPtr obj, int start, int continue_offset);
ObjPtr	_CompileStat (ObjPtr obj, ExprPtr expr, NamespacePtr namespace);
ObjPtr	CompileFunc (ObjPtr obj, CodePtr code, NamespacePtr namespace, ExprPtr stat);
ObjPtr	CompileDecl (ObjPtr obj, ExprPtr decls, NamespacePtr namespace);
ObjPtr	CompileFuncCode (CodePtr code, NamespacePtr namespace, ExprPtr stat);
void	CompileError (ObjPtr obj, ExprPtr stat, char *s, ...);

static void
CompileCanonType (ObjPtr obj, NamespacePtr namespace, TypesPtr type, ExprPtr stat)
{
    SymbolPtr	s;
    int		depth;
    ArgType	*arg;
    int		n;
    
    if (!type)
	return;
    switch (type->base.tag) {
    case types_prim:
	break;
    case types_name:
	if (!type->name.type)
	{
	    s = NamespaceFindSymbol (namespace, type->name.name, &depth);
	    if (!s)
		CompileError (obj, stat, "No typedef \"%A\" in namespace",
			      type->name.name);
	    else if (s->symbol.class != class_typedef)
		CompileError (obj, stat, "Symbol \"%A\" not a typedef",
			      type->name.name);
	    else
	    {
		type->name.type = s->symbol.type;
/*		CompileCanonType (obj, namespace, type->name.type, stat); */
	    }
	}
	break;
    case types_ref:
    	CompileCanonType (obj, namespace, type->ref.ref, stat);
	break;
    case types_func:
	CompileCanonType (obj, namespace, type->func.ret, stat);
	for (arg = type->func.args; arg; arg = arg->next)
	    CompileCanonType (obj, namespace, arg->type, stat);
	break;
    case types_array:
	CompileCanonType (obj, namespace, type->array.type, stat);
	break;
    case types_struct:
	for (n = 0; n < type->structs.structs->nelements; n++)
	{
	    StructElement   *se;

	    se = &StructTypeElements(type->structs.structs)[n];
	    CompileCanonType (obj, namespace, se->type, stat);
	}
	break;
    }
}

/*
 * Add a symbol to a namespace while also updating storage
 * information
 */
static SymbolPtr
CompileAddSymbol (NamespacePtr namespace, SymbolPtr symbol)
{
    ENTER ();
    
    NamespaceAddSymbol (namespace, symbol);
    /*
     * For symbols hanging from a frame (statics, locals and args),
     * locate the frame and set their element value
     */
    if (ClassFrame(symbol->symbol.class))
    {
	while (!namespace->code)
	    namespace = namespace->previous;
	switch (symbol->symbol.class) {
	case class_static:
	    symbol->local.element = AddBoxTypes (&namespace->code->func.statics,
						 symbol->symbol.type);
	    break;
	case class_arg:
	    namespace->code->base.argc++;
	case class_auto:
	    symbol->local.element = AddBoxTypes (&namespace->code->func.dynamics,
						 symbol->symbol.type);
	    break;
	default:
	    break;
	}
    }
    RETURN (symbol);
}

/*
 * Create a new symbol in the specified namespace, as a special
 * case, re-use symbols if not inside a function, but require
 * that the types match exactly
 */
static SymbolPtr
CompileNewSymbol (ObjPtr obj, ExprPtr stat, NamespacePtr namespace,
		   Publish publish, Class class, Types *type, Atom name,
		   Bool *new)
{
    ENTER ();
    SymbolPtr	s = 0;
    
    if (publish == publish_extend)
    {
	s = NamespaceLookupSymbol (namespace, name);
	*new = False;
	if (!s)
	{
	    CompileError (obj, stat, "No namespace \"%A\" in namespace", name);
	    RETURN (0);
	}
    } 

    if (class == class_typedef)
    {
	s = NamespaceLookupSymbol (namespace, name);
	if (s)
	{
	    if (s->symbol.class == class_typedef &&
		s->symbol.type == 0)
	    {
		*new = False;
		s->symbol.type = type;
	    }
	    else
		s = 0;
	}
    }
    if (!s)
    {
	switch (class) {
	case class_global:
	    s = NewSymbolGlobal (name, type, publish);
	    break;
	case class_static:
	    s = NewSymbolStatic (name, type);
	    break;
	case class_arg:
	case class_auto:
	    s = NewSymbolAuto (name, type);
	    break;
	case class_typedef:
	    s = NewSymbolType (name, type, publish);
	    break;
	case class_namespace:
	    s = NewSymbolNamespace (name, publish);
	    break;
	case class_exception:
	    s = NewSymbolException (name, type, publish);
	    break;
	default:
	    s = 0;
	    break;
	}
	*new = s != 0;
    }
    CompileCanonType (obj, namespace, type, stat);
    RETURN (s);
}

/*
 * Find a symbol, creating one if necessary
 * using defaults for class and publication
 */
static SymbolPtr
CompileFindSymbol (ObjPtr obj, ExprPtr stat, NamespacePtr namespace, Atom name, 
		   int *depth, Bool createIfNecessary)
{
    ENTER ();
    SymbolPtr   s;
    Bool	new;

    s = NamespaceFindSymbol (namespace, name, depth);
    if (!s)
    {
	if (!createIfNecessary)
	{
	    CompileError (obj, stat, "No symbol \"%A\" in namespace", name);
	    RETURN (0);
	}
	if (!s)
	{
	    s = CompileNewSymbol (obj, stat, namespace,
				   publish_private, NamespaceDefaultClass (namespace),
				   typesPoly, name, &new);
	    if (new)
		CompileAddSymbol (namespace, s);
	}
	*depth = 0;
    }
    /*
     * For args and autos, make sure we're not compiling a static
     * initializer, in that case, the locals will not be in dynamic
     * namespace
     */
    if (ClassLocal (s->symbol.class))
    {
	while (namespace && !namespace->code)
	    namespace = namespace->previous;
	if (namespace && namespace->code->func.inStaticInit)
	{
	    CompileError (obj, stat, "Symbol \"%A\" not in initializer namespace",
			  name);
	}
    }
    RETURN (s);
}

void
CompileError (ObjPtr obj, ExprPtr stat, char *s, ...)
{
    va_list args;

    FilePuts (FileStderr, "-> ");
    PrintStat (FileStderr, stat, False);
    FilePuts (FileStderr, "Error: ");
    va_start (args, s);
    vPrintError (s, args);
    va_end (args);
    obj->errors++;
}

/*
 * Compile the left side of an assignment statement.
 * The result is a 'ref' left in the value register
 */
ObjPtr
CompileLvalue (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, ExprPtr stat,
		Bool createIfNecessary)
{
    ENTER ();
    InstPtr	inst;
    SymbolPtr	s;
    int		depth;
    
    expr->base.namespace = namespace;
    switch (expr->base.tag) {
    case NAME:
	s = CompileFindSymbol (obj, stat, namespace, expr->atom.atom, 
			       &depth, createIfNecessary);
	if (!s)
	    break;
	if (!ClassStorage (s->symbol.class))
	{
	    CompileError (obj, stat, "Invalid use of %C \"%A\"",
			  s->symbol.class, expr->atom.atom);
	
	    break;
	}
	BuildInst(obj, OpNameRef, inst, stat);
	inst->var.name = s;
	inst->var.staticLink = depth;
	expr->base.type = s->symbol.type;
	break;
    case COLONCOLON:
	s = CompileNamespace (expr->tree.left, namespace);
	if (!s)
	{
	    CompileError (obj, stat, "Object left of ':' is not a namespace");
	    break;
	}
	obj = CompileLvalue (obj, expr->tree.right,
			     s->namespace.namespace, stat, False);
	expr->base.type = expr->tree.right->base.type;
        break;
    case DOT:
	obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
	expr->base.type = TypeCombineStruct (expr->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->atom.atom);
	if (!expr->base.type)
	    CompileError (obj, stat, "Object left of '.' is not a struct");
	BuildInst (obj, OpDotRef, inst, stat);
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case ARROW:
	obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
	BuildInst (obj, OpArrowRef, inst, stat);
	expr->base.type = TypeCombineStruct (expr->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->atom.atom);
	if (!expr->base.type)
	    CompileError (obj, stat, "Object left of '->' is not a struct");
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case OS:
	obj = CompileArray (obj, expr, namespace, OpArrayRef, stat);
	break;
    case STAR:
	obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
	expr->base.type = TypeCombineUnary (expr->tree.left->base.type, expr->base.tag);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible type %t in unary operation",
			  expr->tree.left->base.type);
	}
	break;
    default:
	CompileError (obj, stat, "Invalide lvalue");
	break;
    }
    RETURN (obj);
}

/*
 * Compile a binary operator --
 * compile the left side, push, compile the right and then
 * add the operator
 */
ObjPtr
CompileBinary (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, OpCode opCode, ExprPtr stat)
{
    ENTER ();
    InstPtr inst;
    
    obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
    SetPush (obj);
    obj = _CompileExpr (obj, expr->tree.right, namespace, stat);
    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					 expr->base.tag,
					 expr->tree.right->base.type);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible types %t %t in binary operation",
		      expr->tree.left->base.type,
		      expr->tree.right->base.type);
    }
    BuildInst (obj, opCode, inst, stat);
    RETURN (obj);
}

/*
 * Unaries are easy --
 * compile the operand and add the operator
 */
ObjPtr
CompileUnary (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, OpCode opCode, ExprPtr stat)
{
    ENTER ();
    InstPtr inst;
    ExprPtr down;
    
    if (expr->tree.right)
	down = expr->tree.right;
    else
	down = expr->tree.left;
    obj = _CompileExpr (obj, down, namespace, stat);
    expr->base.type = TypeCombineUnary (down->base.type, expr->base.tag);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible type %t in unary operation",
		      down->base.type);
    }
    BuildInst (obj, opCode, inst, stat);
    RETURN (obj);
}

/*
 * Assignement --
 * compile the value, build a ref for the LHS and add the operator
 */
ObjPtr
CompileAssign (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, OpCode opCode, ExprPtr stat)
{
    ENTER ();
    InstPtr inst;
    
    obj = _CompileExpr (obj, expr->tree.right, namespace, stat);
    SetPush (obj);
    obj = CompileLvalue (obj, expr->tree.left, namespace, stat, opCode == OpAssign);
    expr->base.type = TypeCombineAssign (expr->tree.left->base.type,
					 expr->base.tag,
					 expr->tree.right->base.type);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible types in assignment %t = %t",
		      expr->tree.left->base.type,
		      expr->tree.right->base.type);
    }
    BuildInst (obj, opCode, inst, stat);
    RETURN (obj);
}

/*
 * For typechecking, access arguments from left to right
 */

static ExprPtr
CompileGetArg (ExprPtr arg, int i)
{
    while (arg && --i)
	arg = arg->tree.right;
    return arg;
}

static ObjPtr
CompileArgs (ObjPtr obj, int *argcp, ExprPtr arg, NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    int	argc;
    
    argc = 0;
    while (arg)
    {
	obj = _CompileExpr (obj, arg->tree.left, namespace, stat);
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
	CompileError (obj, stat, "Incompatible type %t for call",
		      type);
	EXIT ();
	return False;
    }
    
    if (func_type->base.tag == types_func)
    {
	argt = func_type->func.args;
	i = 0;
	while ((arg = CompileGetArg (args, argc - i)) || 
	       (argt && !argt->varargs))
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
		CompileError (obj, stat, "Incompatible types %t %t argument %d",
			      argt->type, arg->tree.left->base.type, i);
		ret = False;
	    }
	    i++;
	    if (!argt->varargs)
		argt = argt->next;
	}
    }
    EXIT ();
    return ret;
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
CompileCall (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    InstPtr inst;
    int	    argc;

    obj = CompileArgs (obj, &argc, expr->tree.right, namespace, stat);
    obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
    if (!CompileTypecheckArgs (obj, expr->tree.left->base.type,
			       expr->tree.right, argc, stat))
    {
	RETURN (obj);
    }
    expr->base.type = TypeCombineReturn (expr->tree.left->base.type);
    BuildInst (obj, OpCall, inst, stat);
    inst->ints.value = argc;
    BuildInst (obj, OpNoop, inst, stat);
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
CompileRaise (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, ExprPtr stat)
{
    ENTER();
    int		argc;
    int		depth;
    SymbolPtr	sym;
    InstPtr	inst;
    
    sym = CompileFindSymbol (obj, stat, namespace, expr->tree.left->atom.atom,
			     &depth, False);
    if (!sym)
	RETURN (obj);
    obj = CompileArgs (obj, &argc, expr->tree.right, namespace, stat);
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
CompileTwixt (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    int	    enter_inst, twixt_inst, enter_done_inst, leave_done_inst;
    InstPtr inst;
    
    enter_inst = obj->used;

    /* Compile enter expression */
    obj = _CompileExpr (obj, expr->tree.left->tree.left, namespace, stat);
    NewInst (enter_done_inst, obj);
    
    /* here's where the twixt instruction goes */
    NewInst (twixt_inst, obj);

    /* Compile the body */
    obj = _CompileStat (obj, expr->tree.right->tree.left, namespace);
    BuildInst (obj, OpTwixtDone, inst, stat);
    
    /* finish the twixt instruction */
    inst = ObjCode (obj, twixt_inst);
    inst->base.opCode = OpTwixt;
    inst->base.stat = stat;
    inst->twixt.enter = enter_inst - twixt_inst;
    inst->twixt.leave = obj->used - twixt_inst;

    /* Compile leave expression */
    obj = _CompileExpr (obj, expr->tree.left->tree.right, namespace, stat);
    NewInst (leave_done_inst, obj);

    /* finish the enter_done instruction */
    inst = ObjCode (obj, enter_done_inst);
    inst->base.opCode = OpEnterDone;
    inst->base.stat = stat;
    inst->branch.offset = obj->used - enter_done_inst;
    
    /* Compile else */
    if (expr->tree.right->tree.right)
	obj = _CompileStat (obj, expr->tree.right->tree.right, namespace);
    
    /* finish the leave_done instruction */
    inst = ObjCode (obj, leave_done_inst);
    inst->base.opCode = OpLeaveDone;
    inst->base.stat = stat;
    inst->branch.offset = obj->used - leave_done_inst;

    RETURN (obj);
}

ObjPtr
CompileArray (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, OpCode opCode, ExprPtr stat)
{
    ENTER ();
    ExprPtr	sub;
    InstPtr	inst;
    int		ndim;

    sub = expr->tree.right;
    ndim = 0;
    while (sub)
    {
	obj = _CompileExpr (obj, sub->tree.left, namespace, stat);
	if (!TypeCompatible (NewTypesPrim (type_integer),
			     sub->tree.left->base.type,
			     True))
	{
	    CompileError (obj, stat, "Index type %t is not integer",
			  sub->tree.left->base.type);
	    break;
	}
	SetPush (obj);
	sub = sub->tree.right;
	ndim++;
    }
    obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
    expr->base.type = TypeCombineUnary (expr->tree.left->base.type, OS);
    if (!expr->base.type)
	CompileError (obj, stat, "Type %t is not array or string",
		      expr->tree.left->base.type);
    BuildInst (obj, opCode, inst, stat);
    inst->ints.value = ndim;
    RETURN (obj);
}

static ObjPtr
CompileBuildArray (ObjPtr obj, ExprPtr array, ExprPtr sub, 
		   NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    InstPtr	inst;
    int		ndim;

    ndim = 0;
    while (sub)
    {
	obj = _CompileExpr (obj, sub->tree.left, namespace, stat);
	SetPush (obj);
	sub = sub->tree.right;
	ndim++;
    }
    if (array)
	array->base.type = NewTypesArray (typesPoly, sub);
    BuildInst (obj, OpBuildArray, inst, stat);
    inst->ints.value = ndim;
    RETURN (obj);
}

static int
CompileCountDimensions (ExprPtr expr)
{
    int	    ndimMax, ndim;
    
    ndimMax = 0;
    if (expr->base.tag == ARRAY)
    {
	expr = expr->tree.left;
	while (expr)
	{
	    ndim = CompileCountDimensions (expr->tree.left) + 1;
	    if (ndim > ndimMax)
		ndimMax = ndim;
	    expr = expr->tree.right;
	}
    }
    return ndimMax;
}

static void
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
	    if (ndims != 1)
		CompileSizeDimensions (expr->tree.left, dims + 1, ndims - 1);
	    expr = expr->tree.right;
	}
    }
    else
    {
	dim = 1;
	if (ndims != 1)
	    CompileSizeDimensions (expr, dims + 1, ndims - 1);
    }
    if (dim > *dims)
	*dims = dim;
}

static ObjPtr
CompileImplicitArray (ObjPtr obj, ExprPtr array, ExprPtr inits, NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    ExprPtr sub;
    int	    ndim;
    int	    *dims;
    
    ndim = CompileCountDimensions (inits);
    dims = AllocateTemp (ndim * sizeof (int));
    CompileSizeDimensions (inits, dims, ndim);
    sub = 0;
    while (ndim--)
    {
	sub = NewExprTree (COMMA,
			   NewExprConst (NewInt (*dims++)),
			   sub);
    }
    obj = CompileBuildArray (obj, array, sub, namespace, stat);
    RETURN (obj);
}

static ObjPtr
CompileArrayInits (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, int *ninits, ExprPtr stat)
{
    ENTER ();
    if (expr->base.tag == ARRAY)
    {
	expr = expr->tree.left;
	while (expr)
	{
	    obj = CompileArrayInits (obj, expr->tree.left, namespace, ninits, stat);
	    expr = expr->tree.right;
	}
    }
    else
    {
	obj = _CompileExpr (obj, expr, namespace, stat);
	SetPush (obj);
	++(*ninits);
    }
    RETURN (obj);
}

static ObjPtr
CompileStructInitValues (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    if (expr)
    {
	obj = CompileStructInitValues (obj, expr->tree.right, namespace, stat);
	obj = _CompileExpr (obj, expr->tree.left->tree.right, namespace, stat);
	SetPush (obj);
    }
    RETURN (obj);
}

static ObjPtr
CompileStructInitAssigns (ObjPtr obj, ExprPtr expr, StructType *structs,
			   NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    InstPtr inst;
    while (expr)
    {
	BuildInst (obj, OpInitStruct, inst, stat);
	inst->atom.atom = expr->tree.left->tree.left->atom.atom;
	
	expr = expr->tree.right;
    }
    RETURN (obj);
}

static ObjPtr
CompileCatch (ObjPtr obj, ExprPtr catches, ExprPtr body, 
	      NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    int		catch_inst, exception_inst;
    InstPtr	inst;
    ExprPtr	catch;
    SymbolPtr	exception;
    int		depth;
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
	
	exception = CompileFindSymbol (obj, stat, namespace, 
				       catch->code.code->base.name,
				       &depth, False);
	if (!exception)
	    RETURN(obj);
	if (exception->symbol.class != class_exception)
	{
	    CompileError (obj, stat, "Invalid use of %C \"%A\" as exception",
			  exception->symbol.class, exception->symbol.name);
	    RETURN (obj);
	}

	catch_type = NewTypesFunc (typesPoly, catch->code.code->base.args);
	
	if (!TypeCompatible (exception->symbol.type, catch_type, True))
	{
	    CompileError (obj, stat, "Incompatible types %t %t in catch",
			  catch_type, exception->symbol.type);
	    RETURN (obj);
	}
	NewInst (catch_inst, obj);

	obj = CompileFunc (obj, catch->code.code, namespace, stat);
	
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = -1;
	
	NewInst (exception_inst, obj);
    
	inst = ObjCode (obj, catch_inst);
	inst->base.opCode = OpCatch;
	inst->catch.offset = obj->used - catch_inst;
	inst->catch.exception = exception;
    
	obj = CompileCatch (obj, catches->tree.right, body, namespace, stat);
	
	BuildInst (obj, OpEndCatch, inst, stat);
	
	inst = ObjCode (obj, exception_inst);
	inst->base.opCode = OpException;
	inst->branch.offset = obj->used - exception_inst;
    }
    else
	obj = _CompileStat (obj, body, namespace);
    RETURN (obj);
}

SymbolPtr
CompileNamespace (ExprPtr expr, NamespacePtr namespace)
{
    SymbolPtr	s;
    int		depth;
    
    switch (expr->base.tag) {
    case COLONCOLON:
	s = CompileNamespace (expr->tree.left, namespace);
	if (s)
	    return CompileNamespace (expr->tree.right, s->namespace.namespace);
	break;
    case NAME:
	s = NamespaceFindSymbol (namespace, expr->atom.atom, &depth);
	if (s && s->symbol.class == class_namespace)
	    return s;
	break;
    }
    return 0;
}

ObjPtr
_CompileExpr (ObjPtr obj, ExprPtr expr, NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    int	    i;
    int	    test_inst, middle_inst;
    InstPtr inst;
    SymbolPtr	s;
    Types	*t;
    
    expr->base.namespace = namespace;
    switch (expr->base.tag) {
    case NAME:
	BuildInst (obj, OpName, inst, stat);
	s = CompileFindSymbol (obj, stat, namespace, expr->atom.atom,
				&inst->var.staticLink, False);
	if (!s)
	    break;
	if (!ClassStorage (s->symbol.class))
	{
	    CompileError (obj, stat, "Invalid use of %C \"%A\"",
			  s->symbol.class, expr->atom.atom);
	
	    break;
	}
	inst->var.name = s;
	expr->base.type = s->symbol.type;
	break;
    case NEW:
	CompileCanonType (obj, namespace, expr->base.type, stat);
	t = TypesCanon (expr->base.type);
	switch (t->base.tag) {
	case types_struct:
	    if (expr->tree.left)
	    {
		if (expr->tree.left->base.tag != STRUCT)
		{
		    CompileError (obj, stat, "Non struct initializer");
		    break;
		}
		obj = CompileStructInitValues (obj, 
						expr->tree.left->tree.left, 
						namespace, stat);
	    }
	    BuildInst (obj, OpBuildStruct, inst, stat);
	    inst->structs.structs = t->structs.structs;
	    if (expr->tree.left)
	    {
		obj = CompileStructInitAssigns (obj, 
						expr->tree.left->tree.left, 
						t->structs.structs,
						namespace, stat);
	    }
	    break;
	case types_array:
	    if (expr->tree.left)
	    {
		if (expr->tree.left->base.tag != ARRAY)
		{
		    CompileError (obj, stat, "Non array initializer");
		    break;
		}
		i = 0;
		obj = CompileArrayInits (obj, expr->tree.left, 
					 namespace, &i, stat);
	    }
	    if (t->array.dimensions && t->array.dimensions->tree.left)
	    {
		obj = CompileBuildArray (obj, expr, t->array.dimensions,
					 namespace, stat);
	    }
	    else if (!expr->tree.left)
	    {
		CompileError (obj, stat, "Non-dimensioned array with no initializers");
	    }
	    else
	    {
		obj = CompileImplicitArray (obj, expr, expr->tree.left,
					    namespace, stat);
	    }
	    if (expr->tree.left)
	    {
		BuildInst (obj, OpInitArray, inst, stat);
		inst->ints.value = i;
	    }
	    break;
	default:
	    CompileError (obj, stat, 
			  "Non composite type %t in composite initializer", t);
	    break;
	}
	break;
    case CONST:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	expr->base.type = NewTypesPrim (expr->constant.constant->value.tag);
	break;
    case OS:	    
	obj = CompileArray (obj, expr, namespace, OpArray, stat);
	break;
    case OP:	    /* function call */
	obj = CompileCall (obj, expr, namespace, stat);
	break;
    case COLONCOLON:
	s = CompileNamespace (expr->tree.left, namespace);
	if (!s)
	{
	    CompileError (obj, stat, "Object left of ':' is not a namespace");
	    break;
	}
	obj = _CompileExpr (obj, expr->tree.right,
			    s->namespace.namespace, stat);
	expr->base.type = expr->tree.right->base.type;
        break;
    case DOT:
	obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
	expr->base.type = TypeCombineStruct (expr->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->atom.atom);
	if (!expr->base.type)
	    CompileError (obj, stat, "%t is not a struct containing \"%A\"",
			  expr->tree.left->base.type,
			  expr->tree.left->atom.atom);
	BuildInst (obj, OpDot, inst, stat);
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case ARROW:
	obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
	expr->base.type = TypeCombineStruct (expr->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->atom.atom);
	if (!expr->base.type)
	    CompileError (obj, stat, "%t is not a struct ref containing \"%A\"",
			  expr->tree.left->base.type,
			  expr->tree.left->atom.atom);
	BuildInst (obj, OpArrow, inst, stat);
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case FUNCTION:
	obj = CompileFunc (obj, expr->code.code, namespace, stat);
	expr->base.type = NewTypesFunc (expr->code.code->base.type,
					expr->code.code->base.args);
	break;
    case STAR:	    obj = CompileUnary (obj, expr, namespace, OpStar, stat); break;
    case AMPER:	    
	obj = CompileLvalue (obj, expr->tree.left, namespace, stat, False);
	expr->base.type = NewTypesRef (expr->tree.left->base.type);
	break;
    case UMINUS:    obj = CompileUnary (obj, expr, namespace, OpUminus, stat); break;
    case LNOT:	    obj = CompileUnary (obj, expr, namespace, OpLnot, stat); break;
    case BANG:	    obj = CompileUnary (obj, expr, namespace, OpBang, stat); break;
    case FACT:	    obj = CompileUnary (obj, expr, namespace, OpFact, stat); break;
    case INC:	    
	if (expr->tree.left)
	{
	    obj = CompileLvalue (obj, expr->tree.left, namespace, stat, False);
	    expr->base.type = TypeCombineAssign (expr->tree.left->base.type,
						 ASSIGNPLUS,
						 NewTypesPrim (type_int));
	    BuildInst (obj, OpPreInc, inst, stat);
	}
	else
	{
	    obj = CompileLvalue (obj, expr->tree.right, namespace, stat, False);
	    expr->base.type = TypeCombineAssign (expr->tree.right->base.type,
						 ASSIGNPLUS,
						 NewTypesPrim (type_int));
	    BuildInst (obj, OpPostInc, inst, stat);
	}
	if (!expr->base.type)
	    CompileError (obj, stat, "Invalid ++ type %t",
			  expr->tree.left ? expr->tree.left->base.type :
			  expr->tree.right->base.type);
	break;
    case DEC:
	if (expr->tree.left)
	{
	    obj = CompileLvalue (obj, expr->tree.left, namespace, stat, False);
	    expr->base.type = TypeCombineAssign (expr->tree.left->base.type,
						 ASSIGNMINUS,
						 NewTypesPrim (type_int));
	    BuildInst (obj, OpPreDec, inst, stat);
	}
	else
	{
	    obj = CompileLvalue (obj, expr->tree.right, namespace, stat, False);
	    expr->base.type = TypeCombineAssign (expr->tree.right->base.type,
						 ASSIGNMINUS,
						 NewTypesPrim (type_int));
	    BuildInst (obj, OpPostDec, inst, stat);
	}
	if (!expr->base.type)
	    CompileError (obj, stat, "Invalid -- type %t",
			  expr->tree.left ? expr->tree.left->base.type :
			  expr->tree.right->base.type);
	break;
    case PLUS:	    obj = CompileBinary (obj, expr, namespace, OpPlus, stat); break;
    case MINUS:	    obj = CompileBinary (obj, expr, namespace, OpMinus, stat); break;
    case TIMES:	    obj = CompileBinary (obj, expr, namespace, OpTimes, stat); break;
    case DIVIDE:    obj = CompileBinary (obj, expr, namespace, OpDivide, stat); break;
    case DIV:	    obj = CompileBinary (obj, expr, namespace, OpDiv, stat); break;
    case MOD:	    obj = CompileBinary (obj, expr, namespace, OpMod, stat); break;
    case POW:
	obj = _CompileExpr (obj, expr->tree.right, namespace, stat);
	SetPush (obj);
	obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
	SetPush (obj);
	obj = _CompileExpr (obj, NewExprTree (COLONCOLON, BuildName ("Math"), BuildName ("pow")),
			    namespace, stat);
	expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.left->base.type);
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = 2;
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case SHIFTL:    obj = CompileBinary (obj, expr, namespace, OpShiftL, stat); break;
    case SHIFTR:    obj = CompileBinary (obj, expr, namespace, OpShiftR, stat); break;
    case QUEST:
	/*
	 * a ? b : c
	 *
	 * a QUEST b COLON c
	 *   +-------------+
	 *           +-------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
	NewInst (test_inst, obj);
	obj = _CompileExpr (obj, expr->tree.right->tree.left, namespace, stat);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpQuest;
	inst->branch.offset = obj->used - test_inst;
	obj = _CompileExpr (obj, expr->tree.right->tree.right, namespace, stat);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpColon;
	inst->branch.offset = obj->used - middle_inst;
	expr->base.type = TypeCombineBinary (expr->tree.right->tree.left->base.type,
					     COLON,
					     expr->tree.right->tree.right->base.type);
	if (!expr->base.type)
	    CompileError (obj, stat, "Incompatible types %t %t in ?:",
			  expr->tree.right->tree.left->base.type,
			  expr->tree.right->tree.right->base.type);
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case LXOR:	    obj = CompileBinary (obj, expr, namespace, OpLxor, stat); break;
    case LAND:	    obj = CompileBinary (obj, expr, namespace, OpLand, stat); break;
    case LOR:	    obj = CompileBinary (obj, expr, namespace, OpLor, stat); break;
    case AND:
	/*
	 * a && b
	 *
	 * a ANDAND b
	 *   +--------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
	NewInst (test_inst, obj);
	obj = _CompileExpr (obj, expr->tree.right, namespace, stat);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpAnd;
	inst->base.stat = stat;
	inst->branch.offset = obj->used - test_inst;
	expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					     COLON,
					     expr->tree.right->base.type);
	if (!expr->base.type)
	    CompileError (obj, stat, "Incompatible types %t %t in &&",
			  expr->tree.left->base.type,
			  expr->tree.right->base.type);
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case OR:
	/*
	 * a || b
	 *
	 * a OROR b
	 *   +--------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
	NewInst (test_inst, obj);
	obj = _CompileExpr (obj, expr->tree.right, namespace, stat);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpOr;
	inst->base.stat = stat;
	inst->branch.offset = obj->used - test_inst;
	expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					     COLON,
					     expr->tree.right->base.type);
	if (!expr->base.type)
	    CompileError (obj, stat, "Incompatible types %t %t in ||",
			  expr->tree.left->base.type,
			  expr->tree.right->base.type);
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case ASSIGN:	obj = CompileAssign (obj, expr, namespace, OpAssign, stat); break;
    case ASSIGNPLUS:	obj = CompileAssign (obj, expr, namespace, OpAssignPlus, stat); break;
    case ASSIGNMINUS:	obj = CompileAssign (obj, expr, namespace, OpAssignMinus, stat); break;
    case ASSIGNTIMES:	obj = CompileAssign (obj, expr, namespace, OpAssignTimes, stat); break;
    case ASSIGNDIVIDE:	obj = CompileAssign (obj, expr, namespace, OpAssignDivide, stat); break;
    case ASSIGNDIV:	obj = CompileAssign (obj, expr, namespace, OpAssignDiv, stat); break;
    case ASSIGNMOD:	obj = CompileAssign (obj, expr, namespace, OpAssignMod, stat); break;
    case ASSIGNPOW:
	obj = _CompileExpr (obj, expr->tree.right, namespace, stat);
	SetPush (obj);
	obj = CompileLvalue (obj, expr->tree.left, namespace, stat, False);
	SetPush (obj);
	obj = _CompileExpr (obj, NewExprTree (COLONCOLON, BuildName ("Math"), BuildName ("assign_pow")),
			    namespace, stat);
	expr->base.type = TypeCombineAssign (expr->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types in assignment %t = %t",
			  expr->tree.left->base.type,
			  expr->tree.right->base.type);
	}
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = 2;
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case ASSIGNSHIFTL:	obj = CompileAssign (obj, expr, namespace, OpAssignShiftL, stat); break;
    case ASSIGNSHIFTR:	obj = CompileAssign (obj, expr, namespace, OpAssignShiftR, stat); break;
    case ASSIGNLXOR:	obj = CompileAssign (obj, expr, namespace, OpAssignLxor, stat); break;
    case ASSIGNLAND:	obj = CompileAssign (obj, expr, namespace, OpAssignLand, stat); break;
    case ASSIGNLOR:	obj = CompileAssign (obj, expr, namespace, OpAssignLor, stat); break;
    case EQ:	    obj = CompileBinary (obj, expr, namespace, OpEq, stat); break;
    case NE:	    obj = CompileBinary (obj, expr, namespace, OpNe, stat); break;
    case LT:	    obj = CompileBinary (obj, expr, namespace, OpLt, stat); break;
    case GT:	    obj = CompileBinary (obj, expr, namespace, OpGt, stat); break;
    case LE:	    obj = CompileBinary (obj, expr, namespace, OpLe, stat); break;
    case GE:	    obj = CompileBinary (obj, expr, namespace, OpGe, stat); break;
    case COMMA:	    
	obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
	expr->base.type = expr->tree.left->base.type;
	if (expr->tree.right)
	    obj = _CompileExpr (obj, expr->tree.right, namespace, stat);
	    expr->base.type = expr->tree.right->base.type;
	break;
    case FORK:
	BuildInst (obj, OpFork, inst, stat);
	inst->obj.obj = CompileExpr (expr->tree.right, namespace);
	expr->base.type = NewTypesPrim (type_thread);
	break;
    case THREAD:
	obj = CompileCall (obj, NewExprTree (OP, 
					      NewExprAtom (AtomId ("ThreadFromId")),
					      NewExprTree (COMMA, 
							   expr->tree.left,
							   (Expr *) 0)),
			    namespace, stat);
	expr->base.type = NewTypesPrim (type_thread);
	break;
    }
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
	    if (inst->branch.offset == 0 && continue_offset > 0)
		inst->branch.offset = continue_offset - start;
	    break;
	default:
	    break;
	}
	++start;
    }
}

ObjPtr
_CompileStat (ObjPtr obj, ExprPtr expr, NamespacePtr namespace)
{
    ENTER ();
    int		top_inst, continue_inst, test_inst, middle_inst, bottom_inst;
    ExprPtr	c;
    int		ncase, *case_inst, icase;
    Bool	has_default;
    InstPtr	inst;
    SymbolPtr	sym;
    Bool	new;
    
    expr->base.namespace = namespace;
    switch (expr->base.tag) {
    case IF:
	/*
	 * if (a) b
	 *
	 * a IF b
	 *   +----+
	 */
	obj = _CompileExpr (obj, expr->tree.left, namespace, expr);
	NewInst (test_inst, obj);
	obj = _CompileStat (obj, expr->tree.right, namespace);
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
	obj = _CompileExpr (obj, expr->tree.left, namespace, expr);
	NewInst (test_inst, obj);
	obj = _CompileStat (obj, expr->tree.right->tree.left, namespace);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpIf;
	inst->base.stat = expr;
	inst->branch.offset = obj->used - test_inst;
	obj = _CompileStat (obj, expr->tree.right->tree.right, namespace);
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
	obj = _CompileExpr (obj, expr->tree.left, namespace, expr);
	NewInst (test_inst, obj);
	obj = _CompileStat (obj, expr->tree.right, namespace);
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
	obj = _CompileStat (obj, expr->tree.left, namespace);
	obj = _CompileExpr (obj, expr->tree.right, namespace, expr);
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
	    obj = _CompileExpr (obj, expr->tree.left->tree.left, namespace, expr);
	top_inst = obj->used;
	test_inst = -1;
	if (expr->tree.left->tree.right)
	{
	    obj = _CompileExpr (obj, expr->tree.left->tree.right, namespace, expr);
	    NewInst (test_inst, obj);
	}
	obj = _CompileStat (obj, expr->tree.right->tree.right, namespace);
	continue_inst = obj->used;
	if (expr->tree.right->tree.left)
	    obj = _CompileExpr (obj, expr->tree.right->tree.left, namespace, expr);
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
	/*
	 * switch (a) { case b: c; case d: e; }
	 *
	 *  a b CASE d CASE DEFAULT c e
	 *       +------------------+
	 *              +-------------+
	 *                     +--------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, namespace, expr);
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
		obj = _CompileExpr (obj, c->tree.left->tree.left, namespace, c);
		NewInst (case_inst[icase], obj);
		icase++;
	    }
	    c = c->tree.right;
	}
	/* add default case at the bottom */
    	NewInst (test_inst, obj);
	top_inst = obj->used;
	/*
	 * Create an anonymous namespace
	 */
	namespace = NewNamespace (namespace);
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
		inst->base.opCode = OpCase;
		inst->base.stat = expr;
		inst->branch.offset = obj->used - case_inst[icase];
		icase++;
	    }
	    else
	    {
		inst = ObjCode (obj, test_inst);
		inst->base.opCode = OpDefault;
		inst->base.stat = expr;
		inst->branch.offset = obj->used - test_inst;
	    }
	    while (s->tree.left)
	    {
		obj = _CompileStat (obj, s->tree.left, namespace);
		s = s->tree.right;
	    }
	    c = c->tree.right;
	}
	if (!has_default)
	{
	    inst = ObjCode (obj, test_inst);
	    inst->base.opCode = OpDefault;
	    inst->base.stat = expr;
	    inst->branch.offset = obj->used - test_inst;
	}
	CompilePatchLoop (obj, top_inst, -1);
	break;
    case OC:
	/*
	 * Create an anonymous namespace
	 */
	namespace = NewNamespace (namespace);
	while (expr->tree.left)
	{
	    obj = _CompileStat (obj, expr->tree.left, namespace);
	    expr = expr->tree.right;
	}
	break;
    case BREAK:
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpBreak;
	inst->base.stat = expr;
	inst->branch.offset = 0;    /* filled in by PatchLoop */
	break;
    case CONTINUE:
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpContinue;
	inst->base.stat = expr;
	inst->branch.offset = 0;    /* filled in by PatchLoop */
	break;
    case RETURNTOK:
	if (expr->tree.right)
	    obj = _CompileExpr (obj, expr->tree.right, namespace, expr);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpReturn;
	inst->base.stat = expr;
	break;
    case EXPR:
	obj = _CompileExpr (obj, expr->tree.left, namespace, expr);
	break;
    case SEMI:
	break;
    case FUNCTION:
	sym = CompileNewSymbol (obj, expr, namespace,
				 expr->tree.left->decl.publish,
				 NamespaceDefaultClass (namespace),
				 expr->tree.left->decl.type,
				 expr->tree.left->decl.decl->name,
				 &new);
	if (new)
	    CompileAddSymbol (namespace, sym);
	obj = CompileAssign (obj, expr->tree.right, namespace, OpAssign, expr);
	break;
    case VAR:
	obj = CompileDecl (obj, expr, namespace);
	break;
    case NAMESPACE:
	sym = CompileNewSymbol (obj, expr, namespace, 
				 expr->tree.left->decl.publish,
				 class_namespace, 0, 
				 expr->tree.left->decl.decl->name,
				 &new);
	if (!sym)
	    break;
	if (sym->symbol.class != class_namespace)
	{
	    CompileError (obj, expr, "Invalid use of %C \"%A\"",
			  sym->symbol.class,
			  expr->tree.left->decl.decl->name);
	    break;
	}
	if (new)
	    sym->namespace.namespace = NewNamespace (namespace);
        namespace = sym->namespace.namespace;
	/*
	 * Make private symbols in this namespace visible
	 */
	namespace->publish = publish_public;
	expr = expr->tree.right;
	while (expr->tree.left)
	{
	    obj = _CompileStat (obj, expr->tree.left, namespace);
	    expr = expr->tree.right;
	}
	/*
	 * Make private symbols in this namespace not visible to searches
	 */
	namespace->publish = publish_private;
	if (new)
	{
	    CompileAddSymbol (namespace->previous, sym);
	}
	break;
    case IMPORT:
	sym = CompileFindSymbol (obj, expr, namespace,
				  expr->tree.left->decl.decl->name,
				  &top_inst, False);
	if (!sym)
	    break;
	if (sym->symbol.class != class_namespace)
	{
	    CompileError (obj, expr, "Invalid use of %C \"%A\"",
			  sym->symbol.class, expr->atom.atom);
	
	    break;
	}
	sym = NamespaceImport (namespace,
			   sym->namespace.namespace,
			   expr->tree.left->decl.publish);
	if (sym)
	    CompileError (obj, expr, "Import \"%A\" causes redefinition of \"%A\"",
			  expr->tree.left->decl.decl->name,
			  sym->symbol.name);
	break;
    case CATCH:
	obj = CompileCatch (obj, expr->tree.right, expr->tree.left, namespace, expr);
	break;
    case RAISE:
	obj = CompileRaise (obj, expr, namespace, expr);
	break;
    case TWIXT:
	obj = CompileTwixt (obj, expr, namespace, expr);
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
    case OpDefault:
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

ObjPtr
CompileFuncCode (CodePtr code, NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    ObjPtr  obj;
    InstPtr inst;
    int	    i;
    Bool    needReturn;
    
    obj = NewObj (OBJ_INCR);
    obj = _CompileStat (obj, code->func.code, namespace);
    needReturn = False;
    if (!obj->used || ObjCode (obj, ObjLast(obj))->base.opCode != OpReturn)
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
	BuildInst (obj, OpReturn, inst, stat);
#ifdef DEBUG
    ObjDump (obj);
#endif
    RETURN (obj);
}

ObjPtr
CompileFunc (ObjPtr obj, CodePtr code, NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    InstPtr	    inst;
    SymbolPtr	    local;
    ArgType	    *args;
    ObjPtr	    staticInit;

    namespace = NewNamespace (namespace);
    namespace->code = code;
    for (args = code->base.args; args; args = args->next)
    {
        CompileCanonType (obj, namespace, args->type, stat);
	if (args->varargs)
	{
	    local = NewSymbolArg (args->name, 
				  NewTypesArray (args->type,
						 NewExprTree (COMMA,
							      NewExprConst (NewInt (0)),
							      0)));
	    local->local.element = AddBoxTypes (&namespace->code->func.dynamics,
						local->symbol.type);
	    NamespaceAddSymbol (namespace, local);
	}
	else
	{
	    local = NewSymbolArg (args->name, args->type);
	    CompileAddSymbol (namespace, local);
	}
    }
    code->func.obj = CompileFuncCode (code, namespace, stat);
    obj->errors += code->func.obj->errors;
    BuildInst (obj, OpObj, inst, stat);
    inst->code.code = code;
    if ((staticInit = code->func.staticInit))
    {
	SetPush (obj);
	BuildInst (staticInit, OpStaticDone, inst, stat);
	BuildInst (staticInit, OpEnd, inst, stat);
#ifdef DEBUG
	ObjDump (staticInit);
#endif
	code->func.staticInit = staticInit;
	BuildInst (obj, OpStaticInit, inst, stat);
	BuildInst (obj, OpNoop, inst, stat);
	obj->errors += staticInit->errors;
    }
    RETURN (obj);
}

ObjPtr
CompileDecl (ObjPtr obj, ExprPtr decls, NamespacePtr namespace)
{
    ENTER ();
    SymbolPtr	    s;
    DeclListPtr	    decl;
    Class	    class;
    Publish	    publish;
    NamespacePtr	    code_namespace;
    NamespacePtr	    compile_namespace;
    ObjPtr	    *initObj;
    Bool	    initialize = False;
    Bool	    addToNamespace;
    
    decls->base.namespace = namespace;
    class = decls->decl.class;
    if (class == class_undef)
	class = NamespaceDefaultClass (namespace);
    publish = decls->decl.publish;
    compile_namespace = 0;
    switch (class) {
    case class_global:
	/*
	 * Globals are compiled in the static initializer for
	 * the outermost enclosing function.
	 */
	for (code_namespace = namespace; code_namespace; code_namespace = code_namespace->previous)
	{
	    if (code_namespace->debugger)
	    {
		code_namespace = 0;
		break;
	    }
	    if (code_namespace->code)
		compile_namespace = code_namespace;
	}
	break;
    case class_static:
	/*
	 * Statics are compiled in the static initializer for
	 * the nearest enclosing function
	 */
	for (code_namespace = namespace; code_namespace; code_namespace = code_namespace->previous)
	{
	    if (code_namespace->debugger)
	    {
		code_namespace = 0;
		break;
	    }
	    if (code_namespace->code || code_namespace->debugger)
		break;
	}
	compile_namespace = code_namespace;
	break;
    case class_auto:
    case class_arg:
	/*
	 * Autos are compiled where they lie; just make sure a function
	 * exists somewhere to hang them from
	 */
	for (code_namespace = namespace; code_namespace; code_namespace = code_namespace->previous)
	{
	    if (code_namespace->debugger)
	    {
		code_namespace = 0;
		break;
	    }
	    if (code_namespace->code)
		break;
	}
	break;
    default:
	code_namespace = 0;
	break;
    }
    if (ClassFrame (class) && !code_namespace)
    {
	CompileError (obj, decls, "Invalid storage class %C", class);
	RETURN (obj);
    }
    for (decl = decls->decl.decl; decl; decl = decl->next) {
	s = CompileNewSymbol (obj, decls, namespace,
			       decls->decl.publish, class, decls->decl.type,
			       decl->name, &addToNamespace);
	if (!s)
	    break;
	/*
	 * Compile initializers in two steps
	 * compile the expression before placing the
	 * name in namespace, and then compile the assignment
	 */
    	initObj = &obj;
	if (decl->init)
	{
	    if (compile_namespace && compile_namespace->code)
	    {
		CodePtr	code = compile_namespace->code;
		if (!code->func.staticInit)
		    code->func.staticInit = NewObj (OBJ_INCR);
		initObj = &code->func.staticInit;
		code->func.inStaticInit = True;
	    }
	    *initObj = _CompileExpr (*initObj, decl->init, 
				     namespace, decls);
	    if (compile_namespace && compile_namespace->code)
	    {
		CodePtr	code = compile_namespace->code;
		code->func.inStaticInit = False;
	    }
	    SetPush (*initObj);
	    initialize = True;
	}
#if 0
	else if (class != class_typedef) 
	{
	    /*
	     * This code automatically creates composite
	     * objects for automatic variables.  I'm not
	     * sure this is a good idea
	     */
	    Types   *t = TypesCanon (decls->decl.type);
	    InstPtr inst;
	    switch (t->base.tag) {
	    case types_struct:
		if (compile_namespace && compile_namespace->code)
		{
		    CodePtr	code = compile_namespace->code;
		    if (!code->func.staticInit)
			code->func.staticInit = NewObj (OBJ_INCR);
		    initObj = &code->func.staticInit;
		    code->func.inStaticInit = True;
		}
		BuildInst (*initObj, OpBuildStruct, inst, decls);
		inst->structs.structs = t->structs.structs;
		if (compile_namespace && compile_namespace->code)
		{
		    CodePtr	code = compile_namespace->code;
		    code->func.inStaticInit = False;
		}
		SetPush (*initObj);
		initialize = True;
		break;
	    case types_array:
		if (t->array.dimensions) 
		{
		    if (compile_namespace && compile_namespace->code)
		    {
			CodePtr	code = compile_namespace->code;
			if (!code->func.staticInit)
			    code->func.staticInit = NewObj (OBJ_INCR);
			initObj = &code->func.staticInit;
			code->func.inStaticInit = True;
		    }
		    *initObj = CompileBuildArray (*initObj, 0,
						   t->array.dimensions,
						   namespace,
						   decls);
		    if (compile_namespace && compile_namespace->code)
		    {
			CodePtr	code = compile_namespace->code;
			code->func.inStaticInit = False;
		    }
		    SetPush (*initObj);
		    initialize = True;
		}
	    default:
	    }
	}
#endif
	if (addToNamespace)
	    CompileAddSymbol (namespace, s);
	if (initialize)
	{
	    InstPtr inst;
	    
	    *initObj = CompileLvalue (*initObj, NewExprAtom (decl->name),
				       namespace, decls, False);
	    BuildInst (*initObj, OpAssign, inst, decls);
	}
    }
    RETURN (obj);
}

ObjPtr
CompileStat (ExprPtr expr, NamespacePtr namespace)
{
    ENTER ();
    ObjPtr  obj;
    InstPtr inst;

    obj = NewObj (OBJ_INCR);
    obj = _CompileStat (obj, expr, namespace);
    BuildInst (obj, OpEnd, inst, expr);
#ifdef DEBUG
    ObjDump (obj);
#endif
    RETURN (obj);
}

ObjPtr
CompileExpr (ExprPtr expr, NamespacePtr namespace)
{
    ENTER ();
    ObjPtr  obj;
    InstPtr inst;
    ExprPtr stat;

    stat = NewExprTree (EXPR, expr, 0);
    stat->base.namespace = namespace;
    obj = NewObj (OBJ_INCR);
    obj = _CompileExpr (obj, expr, namespace, stat);
    BuildInst (obj, OpEnd, inst, stat);
#ifdef DEBUG
    ObjDump (obj);
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
    "Default",
    "Break",
    "Continue",
    "Return",
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
    /*
     * Expr op codes
     */
    "Name",
    "NameRef",
    "Const",
    "BuildArray",
    "InitArray",
    "BuildStruct",
    "InitStruct",
    "Array",
    "ArrayRef",
    "Call",
    "Dot",
    "DotRef",
    "Arrow",
    "ArrowRef",
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
ObjDump (ObjPtr obj, int indent)
{
    int	    i, j;
    InstPtr inst;
    int	    *branch;
    int	    b;

    branch = AllocateTemp (obj->used * sizeof (int));
    
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
    for (i = 0; i < obj->used; i++)
    {
	inst = ObjCode(obj, i);
	if (branch[i])
	    FilePrintf (FileStdout, "L%d:\n", branch[i]);
	ObjIndent (indent);
	FilePrintf (FileStdout, "%s%s %c ",
		    OpNames[inst->base.opCode],
		    "            " + strlen(OpNames[inst->base.opCode]),
		    inst->base.push ? '^' : ' ');
	switch (inst->base.opCode) {
	case OpCatch:
	    FilePrintf (FileStdout, "%s", AtomName (inst->catch.exception->symbol.name));
	    /* fall through ... */
	case OpIf:
	case OpElse:
	case OpWhile:
	case OpEndWhile:
	case OpDo:
	case OpFor:
	case OpEndFor:
	case OpCase:
	case OpDefault:
	case OpBreak:
	case OpContinue:
	case OpQuest:
	case OpColon:
	case OpAnd:
	case OpOr:
	case OpException:
	case OpLeaveDone:
	    j = i + inst->branch.offset;
	    if (0 <= j && j < obj->used)
		FilePrintf (FileStdout, "branch L%d", branch[j]);
	    else
		FilePrintf (FileStdout, "Broken branch %d", inst->branch.offset);
	    break;
	case OpReturn:
	    break;
	case OpFork:
	    FilePrintf (FileStdout, "\n");
	    ObjDump (inst->obj.obj, indent+1);
	    continue;
	case OpEndCatch:
	    break;
	case OpRaise:
	    FilePrintf (FileStdout, "%s", AtomName (inst->raise.exception->symbol.name));
	    FilePrintf (FileStdout, " argc %d", inst->raise.argc);
	    break;
	case OpTwixt:
	    FilePrintf (FileStdout, "enter %d leave %d",
			inst->twixt.enter, inst->twixt.leave);
	    break;
	case OpName:
	case OpNameRef:
	    FilePrintf (FileStdout, "%s", AtomName (inst->var.name->symbol.name));
	    break;
	case OpConst:
	    FilePrintf (FileStdout, "%v", inst->constant.constant);
	    break;
	case OpCall:
	    FilePrintf (FileStdout, "%d args", inst->ints.value);
	    break;
	case OpBuildArray:
	    FilePrintf (FileStdout, "%d dims", inst->ints.value);
	    break;
	case OpDot:
	case OpDotRef:
	case OpArrow:
	case OpArrowRef:
	    FilePrintf (FileStdout, "%s", AtomName (inst->atom.atom));
	    break;
	case OpObj:
	    FilePrintf (FileStdout, "\n");
	    ObjDump (inst->code.code->func.obj, indent+1);
	    continue;
	default:
	    break;
	}
	FilePrintf (FileStdout, "\n");
    }
}

