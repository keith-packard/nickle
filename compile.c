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
	case OpGlobal:
	case OpGlobalRef:
	case OpGlobalRefStore:
	case OpStatic:
	case OpStaticRef:
	case OpStaticRefStore:
	case OpLocal:
	case OpLocalRef:
	case OpLocalRefStore:
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
	case OpFarJump:
	    MemReference (inst->farJump.farJump);
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
ObjPtr	CompileBinOp (ObjPtr obj, ExprPtr expr, BinaryOp op, ExprPtr stat, CodePtr code);
ObjPtr	CompileBinFunc (ObjPtr obj, ExprPtr expr, BinaryFunc func, ExprPtr stat, CodePtr code);
ObjPtr	CompileUnOp (ObjPtr obj, ExprPtr expr, UnaryOp op, ExprPtr stat, CodePtr code);
ObjPtr	CompileUnFunc (ObjPtr obj, ExprPtr expr, UnaryFunc func, ExprPtr stat, CodePtr code);
ObjPtr	CompileAssign (ObjPtr obj, ExprPtr expr, Bool initialize, ExprPtr stat, CodePtr code);
ObjPtr	CompileAssignOp (ObjPtr obj, ExprPtr expr, BinaryOp op, ExprPtr stat, CodePtr code);
ObjPtr	CompileAssignFunc (ObjPtr obj, ExprPtr expr, BinaryFunc func, ExprPtr stat, CodePtr code);
ObjPtr	CompileArrayIndex (ObjPtr obj, ExprPtr expr, ExprPtr stat, CodePtr code, int *ndimp);
ObjPtr	CompileCall (ObjPtr obj, ExprPtr expr, Tail tail, ExprPtr stat, CodePtr code);
ObjPtr	_CompileExpr (ObjPtr obj, ExprPtr expr, Bool evaluate, ExprPtr stat, CodePtr code);
ObjPtr	_CompileBoolExpr (ObjPtr obj, ExprPtr expr, Bool evaluate, ExprPtr stat, CodePtr code);
void	CompilePatchLoop (ObjPtr obj, int start,
			  int continue_offset,
			  int break_offset);
ObjPtr	_CompileStat (ObjPtr obj, ExprPtr expr, Bool last, CodePtr code);
ObjPtr	CompileFunc (ObjPtr obj, CodePtr code, ExprPtr stat, CodePtr previous, NonLocalPtr nonLocal);
ObjPtr	CompileDecl (ObjPtr obj, ExprPtr decls, Bool evaluate, ExprPtr stat, CodePtr code);
ObjPtr	CompileFuncCode (CodePtr	code,
			 ExprPtr	stat,
			 CodePtr	previous,
			 NonLocalPtr	nonLocal);
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
	    symbol->local.element = AddBoxType (&code->func.statics,
						symbol->symbol.type);
	    symbol->local.staticScope = True;
	    symbol->local.code = code;
	    break;
	case class_arg:
	case class_auto:
	    symbol->local.element = AddBoxType (&CodeBody (code)->dynamics,
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
    OpCode	opCode;
    
    switch (expr->base.tag) {
    case NAME:
	s = CompileCheckSymbol (obj, stat, expr, code,
				&depth, createIfNecessary);
	if (!s)
	{
	    expr->base.type = typePoly;
	    break;
	}
	opCode = OpNoop;
	switch (s->symbol.class) {
	case class_const:
	    if (!initialize) {
		CompileError (obj, stat, "Attempt to assign to static variable \"%A\"",
			      expr->atom.atom);
		expr->base.type = typePoly;
		break;
	    }
	    /* fall through ... */
	case class_global:
	    opCode = OpGlobalRef;
	    break;
	case class_static:
	    opCode = OpStaticRef;
	    break;
	case class_arg:
	case class_auto:
	    opCode = OpLocalRef;
	    break;
	default:
	    CompileError (obj, stat, "Invalid use of %C \"%A\"",
			  s->symbol.class, expr->atom.atom);
	    expr->base.type = typePoly;
	    break;
	}
	if (opCode == OpNoop)
	    break;
	if (assign)
	    opCode++;
        BuildInst(obj, opCode, inst, stat);
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
	    expr->base.type = typePoly;
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
	    expr->base.type = typePoly;
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
	    expr->base.type = typePoly;
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
	    expr->base.type = typePoly;
	    break;
	}
	break;
    default:
	CompileError (obj, stat, "Invalid lvalue");
        expr->base.type = typePoly;
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
CompileBinOp (ObjPtr obj, ExprPtr expr, BinaryOp op, ExprPtr stat, CodePtr code)
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
	expr->base.type = typePoly;
    }
    BuildInst (obj, OpBinOp, inst, stat);
    inst->binop.op = op;
    RETURN (obj);
}

ObjPtr
CompileBinFunc (ObjPtr obj, ExprPtr expr, BinaryFunc func, ExprPtr stat, CodePtr code)
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
	expr->base.type = typePoly;
    }
    BuildInst (obj, OpBinFunc, inst, stat);
    inst->binfunc.func = func;
    RETURN (obj);
}

/*
 * Unaries are easy --
 * compile the operand and add the operator
 */

ObjPtr
CompileUnOp (ObjPtr obj, ExprPtr expr, UnaryOp op, ExprPtr stat, CodePtr code)
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
	expr->base.type = typePoly;
    }
    BuildInst (obj, OpUnOp, inst, stat);
    inst->unop.op = op;
    RETURN (obj);
}

ObjPtr
CompileUnFunc (ObjPtr obj, ExprPtr expr, UnaryFunc func, ExprPtr stat, CodePtr code)
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
	expr->base.type = typePoly;
    }
    BuildInst (obj, OpUnFunc, inst, stat);
    inst->unfunc.func = func;
    RETURN (obj);
}

/*
 * Assignement --
 * compile the value, build a ref for the LHS and add the operator
 */
ObjPtr
CompileAssign (ObjPtr obj, ExprPtr expr, Bool initialize, ExprPtr stat, CodePtr code)
{
    ENTER ();
    InstPtr inst;
    
    obj = CompileLvalue (obj, expr->tree.left, stat, code, True, True, initialize);
    SetPush (obj);
    obj = _CompileExpr (obj, expr->tree.right, True, stat, code);
    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					 expr->base.tag,
					 expr->tree.right->base.type);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible types in assignment '%T' = '%T'",
		      expr->tree.left->base.type,
		      expr->tree.right->base.type);
	expr->base.type = typePoly;
    }
    BuildInst (obj, OpAssign, inst, stat);
    inst->assign.initialize = initialize;
    RETURN (obj);
}

ObjPtr
CompileAssignOp (ObjPtr obj, ExprPtr expr, BinaryOp op, ExprPtr stat, CodePtr code)
{
    ENTER ();
    InstPtr inst;
    
    obj = CompileLvalue (obj, expr->tree.left, stat, code, False, False, False);
    SetPush (obj);
    BuildInst (obj, OpFetch, inst, stat);
    SetPush (obj);
    obj = _CompileExpr (obj, expr->tree.right, True, stat, code);
    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					 expr->base.tag,
					 expr->tree.right->base.type);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible types in assignment '%T' = '%T'",
		      expr->tree.left->base.type,
		      expr->tree.right->base.type);
	expr->base.type = typePoly;
    }
    BuildInst (obj, OpAssignOp, inst, stat);
    inst->binop.op = op;
    RETURN (obj);
}

ObjPtr
CompileAssignFunc (ObjPtr obj, ExprPtr expr, BinaryFunc func, ExprPtr stat, CodePtr code)
{
    ENTER ();
    InstPtr inst;
    
    obj = CompileLvalue (obj, expr->tree.left, stat, code, False, False, False);
    SetPush (obj);
    BuildInst (obj, OpFetch, inst, stat);
    SetPush (obj);
    obj = _CompileExpr (obj, expr->tree.right, True, stat, code);
    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					 expr->base.tag,
					 expr->tree.right->base.type);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible types in assignment '%T' = '%T'",
		      expr->tree.left->base.type,
		      expr->tree.right->base.type);
	expr->base.type = typePoly;
    }
    BuildInst (obj, OpAssignFunc, inst, stat);
    inst->binfunc.func = func;
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
		      Type	*type,
		      ExprPtr	args,
		      int	argc,
		      ExprPtr	stat)
{
    ENTER ();
    Bool	ret = True;
    ArgType	*argt;
    ExprPtr	arg;
    Type	*func_type;
    int		i;
    
    func_type = TypeCombineFunction (type);
    if (!func_type)
    {
	CompileError (obj, stat, "Incompatible type '%T' for call",
		      type);
	EXIT ();
	return False;
    }
    
    if (func_type->base.tag == type_func)
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
extern Bool profiling;

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
	expr->base.type = typePoly;
	RETURN (obj);
    }
    expr->base.type = TypeCombineReturn (expr->tree.left->base.type);
    if (tail == TailAlways || 
	(tail == TailVoid && 
	 TypeCanon (expr->base.type) == typePrim[rep_void]))
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
    expr->base.type = typePoly;
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
	obj = _CompileBoolExpr (obj, expr->tree.left->tree.left, True, stat, code);
    else
    {
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = TrueVal;
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
    CompilePatchLoop (obj, twixt_body_inst, -1, obj->used);
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
    inst->branch.mod = BranchModNone;
    
    /* Compile else */
    if (expr->tree.right->tree.right)
	obj = _CompileStat (obj, expr->tree.right->tree.right, False, code);
    
    /* finish the leave_done instruction */
    inst = ObjCode (obj, leave_done_inst);
    inst->base.opCode = OpLeaveDone;
    inst->base.stat = stat;
    inst->branch.offset = obj->used - leave_done_inst;
    inst->branch.mod = BranchModNone;

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
	if (!TypeCompatible (typePrim[rep_integer],
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
CompileBuildArray (ObjPtr obj, ExprPtr expr, TypePtr type, 
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
CompileImplicitArray (ObjPtr obj, ExprPtr array, TypePtr type,
		      ExprPtr inits, int ndim, 
		      ExprPtr stat, CodePtr code)
{
    ENTER ();
    ExprPtr sub;
    int	    *dims;
    int	    n;
    
    dims = AllocateTemp (ndim * sizeof (int));
    memset (dims, '\0', ndim * sizeof (int));
    if (!CompileSizeDimensions (inits, dims, ndim))
    {
	CompileError (obj, stat, "Implicit dimensioned array with variable initializers");
	RETURN (obj);
    }
    sub = 0;
    for (n = 0; n < ndim; n++)
    {
	sub = NewExprTree (COMMA,
			   NewExprConst (TEN_NUM, NewInt (*dims++)),
			   sub);
    }
    obj = CompileBuildArray (obj, array, type, sub, ndim, stat, code);
    RETURN (obj);
}

static ObjPtr
CompileArrayInit (ObjPtr obj, ExprPtr expr, Type *type, ExprPtr stat, CodePtr code);
    
static ObjPtr
CompileArrayInits (ObjPtr obj, ExprPtr expr, TypePtr type, 
		   int ndim, int *ninits, ExprPtr stat, CodePtr code)
{
    ENTER ();
    
    if (expr->base.tag == ARRAY)
    {
	if (ndim == 0 && type->base.tag == type_array)
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
CompileArrayInit (ObjPtr obj, ExprPtr expr, Type *type, ExprPtr stat, CodePtr code)
{
    ENTER ();
    int	    ndim;
    Type   *sub = type->array.type;
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
	    (ndim < ninitdim && sub->base.tag != type_array))
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
CompileCompositeImplicitInit (Type *type)
{
    ENTER ();
    ExprPtr	    init = 0;
    Type	    *sub;
    int		    dim;
    StructTypePtr   structs;
    StructElement   *se;
    int		    i;
    
    type = TypeCanon (type);
    
    switch (type->base.tag) {
    case type_array:
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
    case type_struct:
	structs = type->structs.structs;
	se = StructTypeElements (structs);
	init = 0;
	for (i = 0; i < structs->nelements; i++)
	{
	    ExprPtr	member;
	    
	    sub = TypeCanon (se[i].type);

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
	TypePtr	type = TypeCanon (se[i].type);

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
CompileStructInit (ObjPtr obj, ExprPtr expr, Type *type,
		   ExprPtr stat, CodePtr code)
{
    ENTER ();
    StructType	*structs = type->structs.structs;
    InstPtr	inst;
    ExprPtr	init;
    Type	*mem_type;
    
    for (; expr; expr = expr->tree.right)
    {
        mem_type = StructMemType (structs, expr->tree.left->tree.left->atom.atom);
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
    Type	*catch_type;
    
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
	{
	    CompileError (obj, stat, "No exception '%A' in scope",
			  name->atom.atom);
	    RETURN(obj);
	}
	if (exception->symbol.class != class_exception)
	{
	    CompileError (obj, stat, "Invalid use of %C \"%A\" as exception",
			  exception->symbol.class, catch->code.code->base.name->atom);
	    RETURN (obj);
	}

	catch_type = NewTypeFunc (typePoly, catch->code.code->base.args);
	if (!TypeCompatible (exception->symbol.type, catch_type, True))
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in catch",
			  catch_type, exception->symbol.type);
	    RETURN (obj);
	}
	NewInst (catch_inst, obj);

	/*
	 * Exception arguments are sitting in value, push
	 * them on the stack
	 */
	BuildInst (obj, OpNoop, inst, stat);
	inst->base.push = True;

	/*
	 * Compile the exception handler and the
	 * call to get to it.
	 */
	catch->code.code->base.func = code->base.func;
	obj = CompileFunc (obj, catch->code.code, stat, code,
			   NewNonLocal (obj->nonLocal, 
					NonLocalCatch, 
					NON_LOCAL_RETURN));
	/*
	 * Patch non local returns inside
	 */
	CompilePatchLoop (obj, catch_inst, -1, -1);
	
	BuildInst (obj, OpExceptionCall, inst, stat);
	
	NewInst (exception_inst, obj);
    
	inst = ObjCode (obj, catch_inst);
	inst->base.opCode = OpCatch;
	inst->base.stat = stat;
	inst->catch.offset = obj->used - catch_inst;
	inst->catch.exception = exception;
    
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalTry, 0);
	
	obj = CompileCatch (obj, catches->tree.right, body, stat, code);
	
	obj->nonLocal = obj->nonLocal->prev;

	BuildInst (obj, OpEndCatch, inst, stat);
	
	inst = ObjCode (obj, exception_inst);
	inst->base.opCode = OpBranch;
	inst->base.stat = stat;
	inst->branch.offset = obj->used - exception_inst;
	inst->branch.mod = BranchModNone;
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
    Type	*t;
    OpCode	opCode;
    int		staticLink;
    
    switch (expr->base.tag) {
    case NAME:
	s = CompileCheckSymbol (obj, stat, expr, code, &staticLink, False);
	if (!s)
	{
	    expr->base.type = typePoly;
	    break;
	}
	switch (s->symbol.class) {
	case class_const:
	case class_global:
	    opCode = OpGlobal;
	    break;
	case class_static:
	    opCode = OpStatic;
	    break;
	case class_arg:
	case class_auto:
	    opCode = OpLocal;
	    break;
	default:
	    CompileError (obj, stat, "Invalid use of %C \"%A\"",
			  s->symbol.class, expr->atom.atom);
	    expr->base.type = typePoly;
	    opCode = OpNoop;
	    break;
	}
	if (opCode == OpNoop)
	    break;
	BuildInst (obj, opCode, inst, stat);
	inst->var.name = s;
	inst->var.staticLink = staticLink;
	expr->base.type = s->symbol.type;
	break;
    case VAR:
	obj = CompileDecl (obj, expr, evaluate, stat, code);
	break;
    case NEW:
	t = TypeCanon (expr->base.type);
	switch (t ? t->base.tag : type_prim) {
	case type_struct:
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
	case type_array:
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
	t = TypeCanon (expr->base.type);
	if (t && t->base.tag == type_union)
	{
	    StructType	*st = t->structs.structs;
	    
	    expr->tree.left->base.type = StructMemType (st, expr->tree.left->atom.atom);
	    if (!expr->tree.left->base.type)
	    {
		CompileError (obj, stat, "Type '%T' is not a union containing \"%A\"",
			      expr->base.type,
			      expr->tree.left->atom.atom);
		break;
	    }
	    BuildInst (obj, OpBuildUnion, inst, stat);
	    inst->structs.structs = st;
	    if (expr->tree.left->base.type == typeEnum)
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
	    expr->base.type = typePoly;
	    break;
	}
	break;
    case TEN_NUM:
    case OCTAL0_NUM:
    case OCTAL_NUM:
    case BINARY_NUM:
    case HEX_NUM:
    case CHAR_CONST:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	/*
	 * Magic - 0 is both integer and *poly
	 */
	if (ValueIsInt(expr->constant.constant) &&
	    expr->constant.constant->ints.value == 0)
	{
	    expr->base.type = typeNil;
	}
	else
	    expr->base.type = typePrim[rep_integer];
	break;
    case TEN_FLOAT:
    case OCTAL_FLOAT:
    case BINARY_FLOAT:
    case HEX_FLOAT:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	expr->base.type = typePrim[rep_rational];
	break;
    case STRING_CONST:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	expr->base.type = typePrim[rep_string];
	break;
    case VOIDVAL:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	expr->base.type = typePrim[rep_void];
	break;
    case BOOLVAL:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	expr->base.type = typePrim[rep_bool];
	break;
    case POLY_CONST:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	expr->base.type = typePoly;    /* FIXME composite const type */
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
	    expr->base.type = typePoly;
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
	    expr->base.type = typePoly;
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
	    expr->base.type = typePoly;
	    break;
	}
	BuildInst (obj, OpArrow, inst, stat);
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case FUNC:
	obj = CompileFunc (obj, expr->code.code, stat, code, 0);
	expr->base.type = NewTypeFunc (expr->code.code->base.type,
					expr->code.code->base.args);
	break;
    case STAR:	    obj = CompileUnFunc (obj, expr, Dereference, stat, code); break;
    case AMPER:	    
	obj = CompileLvalue (obj, expr->tree.left, stat, code, False, False, False);
	expr->base.type = NewTypeRef (expr->tree.left->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Type '%T' cannot be an l-value",
			  expr->tree.left->base.type);
	    expr->base.type = typePoly;
	    break;
	}
	break;
    case UMINUS:    obj = CompileUnOp (obj, expr, NegateOp, stat, code); break;
    case LNOT:	    obj = CompileUnFunc (obj, expr, Lnot, stat, code); break;
    case BANG:	    obj = CompileUnFunc (obj, expr, Not, stat, code); break;
    case FACT:	    obj = CompileUnFunc (obj, expr, Factorial, stat, code); break;
    case INC:	    
	if (expr->tree.left)
	{
	    obj = CompileLvalue (obj, expr->tree.left, stat, code, False, False, False);
	    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
						 ASSIGNPLUS,
						 typePrim[rep_int]);
	    BuildInst (obj, OpPreOp, inst, stat);
	}
	else
	{
	    obj = CompileLvalue (obj, expr->tree.right, stat, code, False, False, False);
	    expr->base.type = TypeCombineBinary (expr->tree.right->base.type,
						 ASSIGNPLUS,
						 typePrim[rep_int]);
	    BuildInst (obj, OpPostOp, inst, stat);
	}
        inst->binop.op = PlusOp;
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible type '%T' in ++",
			  expr->tree.left ? expr->tree.left->base.type :
			  expr->tree.right->base.type);
	    expr->base.type = typePoly;
	    break;
	}
	break;
    case DEC:
	if (expr->tree.left)
	{
	    obj = CompileLvalue (obj, expr->tree.left, stat, code, False, False, False);
	    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
						 ASSIGNMINUS,
						 typePrim[rep_int]);
	    BuildInst (obj, OpPreOp, inst, stat);
	}
	else
	{
	    obj = CompileLvalue (obj, expr->tree.right, stat, code, False, False, False);
	    expr->base.type = TypeCombineBinary (expr->tree.right->base.type,
						 ASSIGNMINUS,
						 typePrim[rep_int]);
	    BuildInst (obj, OpPostOp, inst, stat);
	}
	inst->binop.op = MinusOp;
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible type '%T' in --",
			  expr->tree.left ? expr->tree.left->base.type :
			  expr->tree.right->base.type);
	    expr->base.type = typePoly;
	    break;
	}
	break;
    case PLUS:	    obj = CompileBinOp (obj, expr, PlusOp, stat, code); break;
    case MINUS:	    obj = CompileBinOp (obj, expr, MinusOp, stat, code); break;
    case TIMES:	    obj = CompileBinOp (obj, expr, TimesOp, stat, code); break;
    case DIVIDE:    obj = CompileBinOp (obj, expr, DivideOp, stat, code); break;
    case DIV:	    obj = CompileBinOp (obj, expr, DivOp, stat, code); break;
    case MOD:	    obj = CompileBinOp (obj, expr, ModOp, stat, code); break;
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
	    expr->base.type = typePoly;
	    break;
	}
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = 2;
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case SHIFTL:    obj = CompileBinFunc (obj, expr, ShiftL, stat, code); break;
    case SHIFTR:    obj = CompileBinFunc (obj, expr, ShiftR, stat, code); break;
    case QUEST:
	/*
	 * a ? b : c
	 *
	 * a QUEST b COLON c
	 *   +-------------+
	 *           +-------+
	 */
	obj = _CompileBoolExpr (obj, expr->tree.left, True, stat, code);
	NewInst (test_inst, obj);
	obj = _CompileExpr (obj, expr->tree.right->tree.left, evaluate, stat, code);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpBranchFalse;
	inst->base.stat = stat;
	inst->branch.offset = obj->used - test_inst;
	inst->branch.mod = BranchModNone;
	obj = _CompileExpr (obj, expr->tree.right->tree.right, evaluate, stat, code);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpBranch;
	inst->base.stat = stat;
	inst->branch.offset = obj->used - middle_inst;
	inst->branch.mod = BranchModNone;
	expr->base.type = TypeCombineBinary (expr->tree.right->tree.left->base.type,
					     COLON,
					     expr->tree.right->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in ?:",
			  expr->tree.right->tree.left->base.type,
			  expr->tree.right->tree.right->base.type);
	    expr->base.type = typePoly;
	    break;
	}
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case LXOR:	    obj = CompileBinFunc (obj, expr, Lxor, stat, code); break;
    case LAND:	    obj = CompileBinOp (obj, expr, LandOp, stat, code); break;
    case LOR:	    obj = CompileBinOp (obj, expr, LorOp, stat, code); break;
    case AND:
	/*
	 * a && b
	 *
	 * a ANDAND b
	 *   +--------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	NewInst (test_inst, obj);
	obj = _CompileExpr (obj, expr->tree.right, evaluate, stat, code);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpBranchFalse;
	inst->base.stat = stat;
	inst->branch.offset = obj->used - test_inst;
	inst->branch.mod = BranchModNone;
	expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					     AND,
					     expr->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in &&",
			  expr->tree.left->base.type,
			  expr->tree.right->base.type);
	    expr->base.type = typePoly;
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
	obj = _CompileExpr (obj, expr->tree.right, evaluate, stat, code);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpBranchTrue;
	inst->base.stat = stat;
	inst->branch.offset = obj->used - test_inst;
	inst->branch.mod = BranchModNone;
	expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					     OR,
					     expr->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types '%T', '%T' in ||",
			  expr->tree.left->base.type,
			  expr->tree.right->base.type);
	    expr->base.type = typePoly;
	    break;
	}
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case ASSIGN:	obj = CompileAssign (obj, expr, False, stat, code); break;
    case ASSIGNPLUS:	obj = CompileAssignOp (obj, expr, PlusOp, stat, code); break;
    case ASSIGNMINUS:	obj = CompileAssignOp (obj, expr, MinusOp, stat, code); break;
    case ASSIGNTIMES:	obj = CompileAssignOp (obj, expr, TimesOp, stat, code); break;
    case ASSIGNDIVIDE:	obj = CompileAssignOp (obj, expr, DivideOp, stat, code); break;
    case ASSIGNDIV:	obj = CompileAssignOp (obj, expr, DivOp, stat, code); break;
    case ASSIGNMOD:	obj = CompileAssignOp (obj, expr, ModOp, stat, code); break;
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
	    expr->base.type = typePoly;
	    break;
	}
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = 2;
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case ASSIGNSHIFTL:	obj = CompileAssignFunc (obj, expr, ShiftL, stat, code); break;
    case ASSIGNSHIFTR:	obj = CompileAssignFunc (obj, expr, ShiftR, stat, code); break;
    case ASSIGNLXOR:	obj = CompileAssignFunc (obj, expr, Lxor, stat, code); break;
    case ASSIGNLAND:	obj = CompileAssignOp (obj, expr, LandOp, stat, code); break;
    case ASSIGNLOR:	obj = CompileAssignOp (obj, expr, LorOp, stat, code); break;
    case EQ:	    obj = CompileBinOp (obj, expr, EqualOp, stat, code); break;
    case NE:	    obj = CompileBinFunc (obj, expr, NotEqual, stat, code); break;
    case LT:	    obj = CompileBinOp (obj, expr, LessOp, stat, code); break;
    case GT:	    obj = CompileBinFunc (obj, expr, Greater, stat, code); break;
    case LE:	    obj = CompileBinFunc (obj, expr, LessEqual, stat, code); break;
    case GE:	    obj = CompileBinFunc (obj, expr, GreaterEqual, stat, code); break;
    case COMMA:	    
	obj = _CompileExpr (obj, expr->tree.left, False, stat, code);
	expr->base.type = expr->tree.left->base.type;
	if (expr->tree.right)
	{
	    obj = _CompileExpr (obj, expr->tree.right, evaluate, stat, code);
	    expr->base.type = expr->tree.right->base.type;
	}
	break;
    case FORK:
	BuildInst (obj, OpFork, inst, stat);
	inst->obj.obj = CompileExpr (expr->tree.right, code);
	expr->base.type = typePrim[rep_thread];
	break;
    case THREAD:
	obj = CompileCall (obj, NewExprTree (OP,
					     expr->tree.right,
					     NewExprTree (COMMA, 
							  expr->tree.left,
							  (Expr *) 0)),
			   TailNever,
			   stat, code);
	expr->base.type = typePrim[rep_thread];
	break;
    case DOLLAR:
	/* reposition statement reference so top-level errors are nicer*/
	obj = _CompileExpr (obj, expr->tree.left, evaluate, expr, code);
	expr->base.type = expr->tree.left->base.type;
	break;
    }
    assert (!evaluate || expr->base.type);
    RETURN (obj);
}

void
CompilePatchLoop (ObjPtr    obj, 
		  int	    start,
		  int	    continue_offset,
		  int	    break_offset)
{
    InstPtr inst;

    while (start < obj->used)
    {
	inst = ObjCode (obj, start);
	switch (inst->base.opCode) {
	case OpBranch:
	    if (inst->branch.offset == 0)
	    {
		switch (inst->branch.mod) {
		case BranchModBreak:
		    inst->branch.offset = break_offset - start;
		    break;
		case BranchModContinue:
		    if (continue_offset >= 0)
			inst->branch.offset = continue_offset - start;
		    break;
		default:
		}
	    }
	    break;
	case OpFarJump:
	    if (inst->farJump.farJump->inst == -1)
	    {
		switch (inst->farJump.mod) {
		case BranchModBreak:
		    inst->farJump.farJump->inst = break_offset;
		    break;
		case BranchModContinue:
		    inst->farJump.farJump->inst = continue_offset;
		    break;
		case BranchModReturn:
		case BranchModReturnVoid:
		    inst->farJump.farJump->inst = -2;
		    break;
		case BranchModNone:
		    break;
		}
	    }
	    break;
	case OpObj:
	    if (!inst->code.code->base.builtin &&
		inst->code.code->func.body.obj->nonLocal)
	    {
		if (inst->code.code->func.body.obj)
		    CompilePatchLoop (inst->code.code->func.body.obj, 0,
				      continue_offset,
				      break_offset);
		if (inst->code.code->func.staticInit.obj)
		    CompilePatchLoop (inst->code.code->func.staticInit.obj, 0,
				      continue_offset,
				      break_offset);
	    }
	    break;
	default:
	    break;
	}
	++start;
    }
}

static ObjPtr
_CompileNonLocal (ObjPtr obj, BranchMod mod, ExprPtr expr, CodePtr code)
{
    ENTER ();
    int		twixt = 0, catch = 0, frame = 0;
    NonLocal	*nl;
    InstPtr	inst;
    int		target;

    switch (mod) {
    case BranchModBreak:	target = NON_LOCAL_BREAK; break;
    case BranchModContinue:	target = NON_LOCAL_CONTINUE; break;
    case BranchModReturn:
    case BranchModReturnVoid:	target = NON_LOCAL_RETURN; break;
    case BranchModNone:		
    default:			RETURN(obj);
    }
    for (nl = obj->nonLocal; nl; nl = nl->prev)
    {
	if (nl->target & target)
	    break;
	switch (nl->kind) {
	case NonLocalTwixt:
	    twixt++;
	    break;
	case NonLocalTry:
	    catch++;
	    break;
	case NonLocalCatch:
	    frame++;
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
	case NON_LOCAL_RETURN:
	    break;
	}
    }
    if (twixt || catch || frame)
    {
	BuildInst (obj, OpFarJump, inst, expr);
	inst->base.stat = expr;
	inst->farJump.farJump = 0;
	inst->farJump.farJump = NewFarJump (-1, twixt, catch, frame);
	inst->farJump.mod = mod;
    }
    else
    {
	switch (mod) {
	case BranchModReturn:
	    BuildInst (obj, OpReturn, inst, expr);
	    break;
	case BranchModReturnVoid:
	    BuildInst (obj, OpReturnVoid, inst, expr);
	    break;
	case BranchModBreak:
	case BranchModContinue:
	    BuildInst (obj, OpBranch, inst, expr);
	    inst->branch.offset = 0;	/* filled in by PatchLoop */
	    inst->branch.mod = mod;
	case BranchModNone:
	    break;
	}
    }
    RETURN (obj);
}

static Bool
_CompileCanTailCall (ObjPtr obj, CodePtr code)
{
    return obj->nonLocal == 0 && (!code || code->base.func == code);
}

ObjPtr
_CompileBoolExpr (ObjPtr obj, ExprPtr expr, Bool evaluate, ExprPtr stat, CodePtr code)
{
    obj = _CompileExpr (obj, expr, evaluate, stat, code);
    if (!TypePoly (expr->base.type) && !TypeBool (expr->base.type))
    {
	CompileError (obj, expr, "Conditional is '%T', not bool", expr->base.type);
    }
    return obj;
}

ObjPtr
_CompileStat (ObjPtr obj, ExprPtr expr, Bool last, CodePtr code)
{
    ENTER ();
    int		start_inst, top_inst, continue_inst, test_inst, middle_inst;
    ExprPtr	c;
    int		ncase, *case_inst, icase;
    Bool	has_default;
    InstPtr	inst;
    
    switch (expr->base.tag) {
    case IF:
	/*
	 * if (a) b
	 *
	 * a BRANCHFALSE b
	 *   +-------------+
	 */
	obj = _CompileBoolExpr (obj, expr->tree.left, True, expr, code);
	NewInst (test_inst, obj);
	obj = _CompileStat (obj, expr->tree.right, last, code);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpBranchFalse;
	inst->base.stat = expr;
	inst->branch.offset = obj->used - test_inst;
	inst->branch.mod = BranchModNone;
	break;
    case ELSE:
	/*
	 * if (a) b else c
	 *
	 * a BRANCHFALSE b BRANCH c
	 *   +--------------------+
	 *                 +--------+
	 */
	obj = _CompileBoolExpr (obj, expr->tree.left, True, expr, code);
	NewInst (test_inst, obj);
	obj = _CompileStat (obj, expr->tree.right->tree.left, last, code);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpBranchFalse;
	inst->base.stat = expr;
	inst->branch.offset = obj->used - test_inst;
	inst->branch.mod = BranchModNone;
	obj = _CompileStat (obj, expr->tree.right->tree.right, last, code);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpBranch;
	inst->base.stat = expr;
	inst->branch.offset = obj->used - middle_inst;
	inst->branch.mod = BranchModNone;
	break;
    case WHILE:
	/*
	 * while (a) b
	 *
	 * BRANCH b a BRANCHTRUE
	 * +--------+
	 *        +---+
	 */
	NewInst (start_inst, obj);
	    
        top_inst = obj->used;
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalControl,
				     NON_LOCAL_BREAK|NON_LOCAL_CONTINUE);
	obj = _CompileStat (obj, expr->tree.right, False, code);
	obj->nonLocal = obj->nonLocal->prev;

	inst = ObjCode (obj, start_inst);
	inst->base.opCode = OpBranch;
	inst->base.stat = expr;
	inst->branch.offset = obj->used - start_inst;
	inst->branch.mod = BranchModNone;
	
	continue_inst = obj->used;
	obj = _CompileBoolExpr (obj, expr->tree.left, True, expr, code);
	NewInst (test_inst, obj);

	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpBranchTrue;
	inst->base.stat = expr;
	inst->branch.offset = top_inst - test_inst;
	inst->branch.mod = BranchModNone;
	
	CompilePatchLoop (obj, top_inst, continue_inst, obj->used);
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
	obj = _CompileBoolExpr (obj, expr->tree.right, True, expr, code);
	NewInst (test_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpBranchTrue;
	inst->base.stat = expr;
	inst->branch.offset = continue_inst - test_inst;
	inst->branch.mod = BranchModNone;
	CompilePatchLoop (obj, continue_inst, continue_inst, obj->used);
	break;
    case FOR:
	/*
	 * for (a; b; c) d
	 *
	 * a BRANCH d c b BRANCHTRUE
	 *   +----------+
	 *          +-----+
	 */
	/* a */
	if (expr->tree.left->tree.left)
	    obj = _CompileExpr (obj, expr->tree.left->tree.left, False, expr, code);
	
	test_inst = -1;
	start_inst = -1;
	
	/* check for b */
	if (expr->tree.left->tree.right)
	    NewInst (start_inst, obj);

	top_inst = obj->used;

	/* d */
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalControl,
				     NON_LOCAL_BREAK|NON_LOCAL_CONTINUE);
	obj = _CompileStat (obj, expr->tree.right->tree.right, False, code);
	obj->nonLocal = obj->nonLocal->prev;

	/* c */
	continue_inst = obj->used;
	if (expr->tree.right->tree.left)
	    obj = _CompileExpr (obj, expr->tree.right->tree.left, False, expr, code);
	
	/* b */
	if (expr->tree.left->tree.right)
	{
	    /* patch start branch */
	    inst = ObjCode (obj, start_inst);
	    inst->base.opCode = OpBranch;
	    inst->base.stat = expr;
	    inst->branch.offset = obj->used - start_inst;
	    inst->branch.mod = BranchModNone;
	    
	    obj = _CompileBoolExpr (obj, expr->tree.left->tree.right, True, expr, code);
	    NewInst (test_inst, obj);
	    inst = ObjCode (obj, test_inst);
	    inst->base.opCode = OpBranchTrue;
	}
	else
	{
	    NewInst (test_inst, obj);
	    inst = ObjCode (obj, test_inst);
	    inst->base.opCode = OpBranch;
	}
	inst->base.stat = expr;
	inst->branch.offset = top_inst - test_inst;
	inst->branch.mod = BranchModNone;

	CompilePatchLoop (obj, top_inst, continue_inst, obj->used);
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
		    inst->branch.mod = BranchModNone;
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
		    inst->base.opCode = OpBranch;
		inst->base.stat = expr;
		inst->branch.offset = obj->used - test_inst;
		inst->branch.mod = BranchModNone;
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
		inst->base.opCode = OpBranch;
	    inst->base.stat = expr;
	    inst->branch.offset = obj->used - test_inst;
	    inst->branch.mod = BranchModNone;
	}
	CompilePatchLoop (obj, top_inst, -1, obj->used);
	break;
    case FUNC:
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
	obj = _CompileNonLocal (obj, BranchModBreak, expr, code);
	break;
    case CONTINUE:
	obj = _CompileNonLocal (obj, BranchModContinue, expr, code);
	break;
    case RETURNTOK:
	if (!code || !code->base.func)
	{
	    CompileError (obj, expr, "return not in function");
	    break;
	}
	if (expr->tree.right)
	{
	    if (expr->tree.right->base.tag == OP && 
		_CompileCanTailCall (obj, code))
	    {
		obj = CompileCall (obj, expr->tree.right, TailAlways, expr, code);
	    }
	    else
	    {
		obj = _CompileExpr (obj, expr->tree.right, True, expr, code);
		obj = _CompileNonLocal (obj, BranchModReturn, expr, code);
	    }
	    expr->base.type = expr->tree.right->base.type;
	}
	else
	{
	    obj = _CompileNonLocal (obj, BranchModReturnVoid, expr, code);
	    expr->base.type = typePrim[rep_void];
	}
	if (!TypeCombineBinary (code->base.func->base.type, ASSIGN, expr->base.type))
	{
	    CompileError (obj, expr, "Incompatible types '%T', '%T' in return",
			  code->base.type, expr->base.type);
	    break;
	}
	break;
    case EXPR:
	if (last && expr->tree.left->base.tag == OP && !profiling)
	    obj = CompileCall (obj, expr->tree.left, TailVoid, expr, code);
	else
	    obj = _CompileExpr (obj, expr->tree.left, False, expr, code);
	break;
    case SEMI:
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
    case OpBranch:
    case OpBranchFalse:
    case OpBranchTrue:
    case OpFarJump:
    case OpCase:
    case OpTagCase:
    case OpDefault:
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
CompileFuncCode (CodePtr	code,
		 ExprPtr	stat,
		 CodePtr	previous,
		 NonLocalPtr	nonLocal)
{
    ENTER ();
    ObjPtr  obj;
    InstPtr inst;
    int	    i;
    Bool    needReturn;
    
    code->base.previous = previous;
    obj = NewObj (OBJ_INCR);
    obj->nonLocal = nonLocal;
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
CompileFunc (ObjPtr	    obj,
	     CodePtr	    code,
	     ExprPtr	    stat,
	     CodePtr	    previous,
	     NonLocalPtr    nonLocal)
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
    code->func.body.obj = CompileFuncCode (code, stat, previous, nonLocal);
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
    OpCode	    opCode;
    
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
	opCode = OpGlobal;
	break;
    case class_static:
	/*
	 * Statics are compiled in the static initializer for
	 * the nearest enclosing function
	 */
	code_compile = code;
	opCode = OpStatic;
	break;
    case class_auto:
    case class_arg:
	/*
	 * Autos are compiled where they lie; just make sure a function
	 * exists somewhere to hang them from
	 */
	opCode = OpLocal;
	break;
    default:
	opCode = OpNoop;
	break;
    }
    if (ClassFrame (class) && !code)
    {
	CompileError (obj, decls, "Invalid storage class %C", class);
	decls->base.type = typePoly;
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
	    /*
	     * Assign it
	     */
	    lvalue = NewExprAtom (decl->name, decl->symbol, False);
	    *initObj = CompileLvalue (*initObj, lvalue,
				       decls, code, False, True, True);
	    SetPush (*initObj);
	    *initObj = _CompileExpr (*initObj, init, 
				     True, stat, code);
	    if (code_compile)
	    {
		code_compile->func.inStaticInit = False;
		code->func.inGlobalInit = False;
	    }
	    
	    if (!TypeCombineBinary (lvalue->base.type,
				    ASSIGN,
				    init->base.type))
	    {
		CompileError (obj, decls, "Incompatible types '%T', '%T' in initialization",
			      lvalue->base.type,
			      init->base.type);
	    }
	    BuildInst (*initObj, OpAssign, inst, stat);
	    inst->assign.initialize = True;
	}
    }
    if (evaluate)
    {
	if (s)
	{
	    InstPtr	inst;
	    BuildInst (obj, opCode, inst, stat);
	    inst->var.name = s;
	    decls->base.type = s->symbol.type;
	}
	else
	    decls->base.type = typePoly;
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
    "Branch",
    "BranchFalse",
    "BranchTrue",
    "Case",
    "TagCase",
    "Default",
    "Return",
    "ReturnVoid",
    "Fork",
    "Catch",
    "EndCatch",
    "Raise",
    "OpTwixt",
    "OpTwixtDone",
    "OpEnterDone",
    "OpLeaveDone",
    "OpFarJump",
    /*
     * Expr op codes
     */
    "Global",
    "GlobalRef",
    "GlobalRefStore",
    "Static",
    "StaticRef",
    "StaticRefStore",
    "Local",
    "LocalRef",
    "LocalRefStore",
    "Fetch",
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
    "ExceptionCall",
    "Dot",
    "DotRef",
    "DotRefStore",
    "Arrow",
    "ArrowRef",
    "ArrowRefStore",
    "Obj",
    "StaticInit",
    "StaticDone",
    "BinOp",
    "BinFunc",
    "UnOp",
    "UnFunc",
    "PreOp",
    "PostOp",
    "Assign",
    "AssignOp",
    "AssignFunc",
    "End",
};

static char *
ObjBinFuncName (BinaryFunc func)
{
    static struct {
	BinaryFunc  func;
	char	    *name;
    } funcs[] = {
	{ ShiftL,	"ShiftL" },
	{ ShiftR,	"ShiftR" },
	{ Lxor,		"Lxor" },
	{ NotEqual,	"NotEqual" },
	{ Greater,	"Greater" },
	{ LessEqual,	"LessEqual" },
	{ GreaterEqual,	"GreaterEqual" },
	{ 0, 0 }
    };
    int	i;

    for (i = 0; funcs[i].func; i++)
	if (funcs[i].func == func)
	    return funcs[i].name;
    return "<unknown>";
}

static char *
ObjUnFuncName (UnaryFunc func)
{
    static struct {
	UnaryFunc   func;
	char	    *name;
    } funcs[] = {
	{ Dereference,	"Dereference" },
	{ Lnot,		"Lnot" },
	{ Not,		"Not" },
	{ Factorial,	"Factorial" },
	{ 0, 0 }
    };
    int	i;

    for (i = 0; funcs[i].func; i++)
	if (funcs[i].func == func)
	    return funcs[i].name;
    return "<unknown>";
}

static void
ObjIndent (int indent)
{
    int	j;
    for (j = 0; j < indent; j++)
        FilePrintf (FileStdout, "    ");
}

static char *
BranchModName (BranchMod mod)
{
    switch (mod) {
    case BranchModNone:		return "BranchModNone";
    case BranchModBreak:	return "BranchModBreak";
    case BranchModContinue:	return "BranchModContinue";
    case BranchModReturn:	return "BranchModReturn";
    case BranchModReturnVoid:	return "BranchModReturnVoid";
    }
    return "?";
}

void
InstDump (InstPtr inst, int indent, int i, int *branch, int maxbranch)
{
    int	    j;
    Bool    realBranch = False;
    
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
    case OpBranch:
    case OpBranchFalse:
    case OpBranchTrue:
    case OpCase:
    case OpDefault:
    case OpLeaveDone:
	realBranch = True;
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
	if (realBranch)
	    FilePrintf (FileStdout, " %s", 
			BranchModName (inst->branch.mod));
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
    case OpFarJump:
	FilePrintf (FileStdout, "twixt %d catch %d frame %d inst %d mod %s\n",
		    inst->farJump.farJump->twixt,
		    inst->farJump.farJump->catch,
		    inst->farJump.farJump->frame,
		    inst->farJump.farJump->inst,
		    BranchModName (inst->farJump.mod));
	break;
    case OpGlobal:
    case OpGlobalRef:
    case OpGlobalRefStore:
    case OpStatic:
    case OpStaticRef:
    case OpStaticRefStore:
    case OpLocal:
    case OpLocalRef:
    case OpLocalRefStore:
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
    case OpBinOp:
    case OpPreOp:
    case OpPostOp:
    case OpAssignOp:
	FilePrintf (FileStdout, "%O", inst->binop.op);
	break;
    case OpBinFunc:
    case OpAssignFunc:
	FilePrintf (FileStdout, "%s", ObjBinFuncName (inst->binfunc.func));
	break;
    case OpUnOp:
	FilePrintf (FileStdout, "%U", inst->unop.op);
	break;
    case OpUnFunc:
	FilePrintf (FileStdout, "%s", ObjUnFuncName (inst->unfunc.func));
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
    memset (branch, '\0', obj->used * sizeof (int));
    
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
	if (inst->base.stat && inst->base.stat != stat)
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

