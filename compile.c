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
	default:
	    break;
	}
	MemReference (inst->base.stat);
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
#define SetPush(_o)  (ObjCode((_o), ObjLast(_o))->base.push = True);

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

static Class
DefaultClass (NamespacePtr namespace)
{
    while (namespace && !namespace->code && !namespace->debugger)
	namespace = namespace->previous;
    if (namespace && !namespace->debugger)
	return class_auto;
    return class_global;
}

static void
CompileCanonType (ObjPtr obj, NamespacePtr namespace, TypesPtr type, ExprPtr stat)
{
    SymbolPtr	s;
    int		depth;
    ArgType	*arg;
    
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
		CompileCanonType (obj, namespace, type->name.type, stat);
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
    SymbolPtr	s;
    
    CompileCanonType (obj, namespace, type, stat);
    /*
     * A bit of a special case here --
     * redefining in the global scope is ignored, as long
     * as the types match.
     */
    if (DefaultClass (namespace) == class_global)
    {
	s = NamespaceLookupSymbol (namespace, name);
	if (s)
	{
	    if (s->symbol.class != class ||
		!TypeEqual (s->symbol.type, type) ||
		s->symbol.publish != publish)
	    {
		CompileError (obj, stat, "Cannot redefine \"%A\" from %P %C %t to %P %C %t",
			      name,
			      s->symbol.publish, s->symbol.class, s->symbol.type,
			      publish, class, type);
		RETURN (0);
	    }
	    *new = False;
	    RETURN (s);
	}
    }

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
    RETURN (s);
}

/*
 * Find a symbol, creating one if necessary
 * using defaults for class and publication
 */
static SymbolPtr
CompileFindSymbol (ObjPtr obj, ExprPtr stat, NamespacePtr namespace, Atom name, 
		    int *depth, Types *type, Bool createIfNecessary)
{
    ENTER ();
    SymbolPtr   s;
    Bool	new;

    s = NamespaceFindSymbol (namespace, name, depth);
    if (!s)
    {
	CompileCanonType (obj, namespace, type, stat);
	if (!createIfNecessary)
	{
	    CompileError (obj, stat, "No symbol \"%A\" in namespace", name);
	    RETURN (0);
	}
	if (!s)
	{
	    s = CompileNewSymbol (obj, stat, namespace,
				   publish_private, DefaultClass (namespace),
				   type, name, &new);
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
			       &depth, typesPoly, createIfNecessary);
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
    case DOT:
	/*
	 * Yurg -- check for namespace reference
	 */
	if ((s = CompileNamespace (expr->tree.left, namespace)))
	{
	    obj = CompileLvalue (obj, expr->tree.right,
				  s->namespace.namespace, stat, False);
	    expr->base.type = expr->tree.right->base.type;
	}
	else
	{
	    obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
	    expr->base.type = TypeCombineStruct (expr->tree.left->base.type,
						 expr->base.tag,
						 expr->tree.right->atom.atom);
	    if (!expr->base.type)
		CompileError (obj, stat, "Object left of '.' is not a struct");
	    BuildInst (obj, OpDotRef, inst, stat);
	    inst->atom.atom = expr->tree.right->atom.atom;
	}
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
    ExprPtr arg;
    InstPtr inst;
    int	    argc;
    int	    i;
    ArgType *argt;

    arg = expr->tree.right;
    argc = 0;
    while (arg)
    {
	obj = _CompileExpr (obj, arg->tree.left, namespace, stat);
	SetPush (obj);
	arg = arg->tree.right;
	argc++;
    }
    obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
    expr->base.type = TypeCombineFunction (expr->tree.left->base.type);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible type %t for function call",
		      expr->tree.left->base.type);
    }
    if (expr->tree.left->base.type->base.tag == types_func)
    {
	argt = expr->tree.left->base.type->func.args;
	i = 0;
	while ((arg = CompileGetArg (expr->tree.right, argc - i)) || argt)
	{
	    if (!argt)
	    {
		if (!expr->tree.left->base.type->func.varargs)
		    CompileError (obj, stat, "Too many parameters");
		break;
	    }
	    if (!arg)
	    {
		CompileError (obj, stat, "Too few parameters");
		break;
	    }
	    if (!TypeCompatible (argt->type, arg->tree.left->base.type, True))
	    {
		CompileError (obj, stat, "Incompatible types %t %t argument %d",
			      argt->type, arg->tree.left->base.type, i);
		break;
	    }
	    i++;
	    argt = argt->next;
	}
    }
    BuildInst (obj, OpCall, inst, stat);
    inst->ints.value = argc;
    BuildInst (obj, OpNoop, inst, stat);
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
	SetPush (obj);
	sub = sub->tree.right;
	ndim++;
    }
    obj = _CompileExpr (obj, expr->tree.left, namespace, stat);
    expr->base.type = TypeCombineUnary (expr->tree.left->base.type, OC);
    if (!expr->base.type)
	CompileError (obj, stat, "Object left of '[]' is not an array");
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
CompileTry (ObjPtr obj, ExprPtr catches, ExprPtr body, NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    int	    catch_inst, end_inst;
    InstPtr inst;
    ExprPtr catch;
    Bool    finally;
    
    if (catches)
    {
	catch = catches->tree.left;
	/*
	 * try a catch b
	 *
	 * CATCH a ENDCATCH b
	 *             +------+
	 */
	finally = catch->tree.left->atom.atom == 0;
	
	NewInst (catch_inst, obj);
	
	obj = CompileTry (obj, catches->tree.right, body, namespace, stat);
	
	NewInst (end_inst, obj);
	
	inst = ObjCode (obj, catch_inst);
	inst->base.opCode = OpTry;
	inst->catch.exception = catch->tree.left->atom.atom;
	inst->catch.offset = obj->used - catch_inst;
	
	obj = _CompileStat (obj, catch->tree.right, namespace);
	
	inst = ObjCode (obj, end_inst);
	inst->base.opCode = OpEndTry;
	inst->branch.offset = obj->used - end_inst;
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
    case DOT:
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
				&inst->var.staticLink, typesPoly, False);
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
	    if (t->array.dimensions)
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
    case DOT:
	/*
	 * Yurg -- check for namespace reference -- syntatically
	 * identical to structure member reference, but
	 * semantically backwards
	 */
	if ((s = CompileNamespace (expr->tree.left, namespace)))
	{
	    obj = _CompileExpr (obj, expr->tree.right,
				s->namespace.namespace, stat);
	    expr->base.type = expr->tree.right->base.type;
	}
	else
	{
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
	}
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
					False,
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
    case POW:	    obj = CompileBinary (obj, expr, namespace, OpPow, stat); break;
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
    case ASSIGNPOW:	obj = CompileAssign (obj, expr, namespace, OpAssignPow, stat); break;
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
	    if (inst->branch.offset == 0)
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
				 DefaultClass (namespace),
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
			  sym->symbol.class, expr->atom.atom);
	
	    break;
	}
	if (new)
	{
	    namespace = NewNamespace (namespace);
	    sym->namespace.namespace = namespace;
	}
	expr = expr->tree.right;
	while (expr->tree.left)
	{
	    obj = _CompileStat (obj, expr->tree.left, namespace);
	    expr = expr->tree.right;
	}
	if (new)
	{
	    /*
	     * Make private symbols in this namespace not visible to searches
	     */
	    namespace->publish = publish_private;
	    CompileAddSymbol (namespace->previous, sym);
	}
	break;
    case IMPORT:
	sym = CompileFindSymbol (obj, expr, namespace,
				  expr->tree.left->decl.decl->name,
				  &top_inst, typesPoly, False);
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
    case TRY:
	obj = CompileTry (obj, expr->tree.right, expr->tree.left, namespace, expr);
	break;
    }
    RETURN (obj);
}

ObjPtr
CompileFuncCode (CodePtr code, NamespacePtr namespace, ExprPtr stat)
{
    ENTER ();
    ObjPtr  obj;
    InstPtr inst;
    
    obj = NewObj (OBJ_INCR);
    obj = _CompileStat (obj, code->func.code, namespace);
    if (!obj->used || ObjCode (obj, ObjLast(obj))->base.opCode != OpReturn)
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
	local = NewSymbolArg (args->name, args->type);
	CompileAddSymbol (namespace, local);
    }
    code->func.obj = CompileFuncCode (code, namespace, stat);
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
	class = DefaultClass (namespace);
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
    CompileCanonType (obj, namespace, decls->decl.type, decls);
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
    "Break",
    "Continue",
    "Return",
    "Function",
    "Fork",
    "Try",
    "EndTry",
    "Raise",
    "ReRaise",
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

void
ObjDump (ObjPtr obj)
{
    int	    i;
    InstPtr inst;
    
    FilePrintf (FileStdout, "%d instructions\n", obj->used);
    for (i = 0; i < obj->used; i++)
    {
	inst = ObjCode(obj, i);
	FilePrintf (FileStdout, "    %s%s %c ",
		    OpNames[inst->base.opCode],
		    "          " + strlen(OpNames[inst->base.opCode]),
		    inst->base.push ? '^' : ' ');
	switch (inst->base.opCode) {
	case OpIf:
	case OpElse:
	case OpWhile:
	case OpEndWhile:
	case OpDo:
	case OpFor:
	case OpEndFor:
	case OpBreak:
	case OpContinue:
	case OpQuest:
	case OpColon:
	case OpAnd:
	case OpOr:
	    FilePrintf (FileStdout, "branch %d", inst->branch.offset);
	    break;
	case OpReturn:
	    break;
	case OpFork:
	    FilePrintf (FileStdout, "fork");
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
	default:
	    break;
	}
	FilePrintf (FileStdout, "\n");
    }
}

