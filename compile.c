/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	"gram.h"

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
    for (i = 0; i < obj->used_stat; i++)
	MemReference (ObjStat (obj, i)->stat);
}

DataType    ObjType = { ObjMark, 0, "ObjType" };

static ObjPtr
NewObj (int size, int size_stat)
{
    ENTER ();
    ObjPtr  obj;

    obj = ALLOCATE (&ObjType, 
		    sizeof (Obj) + 
		    size * sizeof (Inst) + 
		    size_stat * sizeof (Stat));
    obj->size = size;
    obj->used = 0;
    obj->size_stat = size_stat;
    obj->used_stat = 0;
    obj->error = False;
    obj->nonLocal = 0;
    RETURN (obj);
}

#define OBJ_INCR	32
#define OBJ_STAT_INCR	16

static ObjPtr
AddInst (ObjPtr obj, ExprPtr stat)
{
    ENTER ();
    ObjPtr  nobj;
    int	    need_stat = 1;
    
    if (obj->used_stat && ObjStat(obj, obj->used_stat - 1)->stat == stat)
	need_stat = 0;
    if (obj->used == obj->size || obj->used_stat + need_stat > obj->size_stat)
    {
	int	nsize = obj->size, nsize_stat = obj->size_stat;
	if (obj->used == obj->size)
	    nsize = obj->size + OBJ_INCR;
	if (obj->used_stat + need_stat > obj->size_stat)
	    nsize_stat = obj->size_stat + OBJ_STAT_INCR;
	nobj = NewObj (nsize, nsize_stat);
	memcpy (ObjCode (nobj, 0), ObjCode (obj, 0), obj->used * sizeof (Inst));
	memcpy (ObjStat (nobj, 0), ObjStat (obj, 0), obj->used_stat * sizeof (Stat));
	nobj->used = obj->used;
	nobj->used_stat = obj->used_stat;
	nobj->error = obj->error;
	nobj->nonLocal = obj->nonLocal;
	obj = nobj;
    }
    if (need_stat)
    {
	StatPtr s = ObjStat (obj, obj->used_stat);
	s->inst = obj->used;
	s->stat = stat;
	obj->used_stat++;
    }
    obj->used++;
    RETURN (obj);
}

static ObjPtr
AppendObj (ObjPtr first, ObjPtr last)
{
    int	    i;
    InstPtr firsti, lasti;

    for (i = 0; i < last->used; i++)
    {
	lasti = ObjCode (last, i);
	first = AddInst (first, ObjStatement (last, lasti));
	firsti = ObjCode (first, ObjLast (first));
	*firsti = *lasti;
    }
    return first;
}

ExprPtr
ObjStatement (ObjPtr obj, InstPtr inst)
{
    int	    i = inst - ObjCode(obj, 0);
    int	    low = 0, high = obj->used_stat - 1;
    
    while (low < high - 1)
    {
	int mid = (low + high) >> 1;
	if (ObjStat(obj,mid)->inst <= i)
	    low = mid;
	else
	    high = mid - 1;
    }
    while (low <= high)
    {
        StatPtr s = ObjStat(obj, high);
	if (s->inst <= i)
	    return s->stat;
	high--;
    }
    return 0;
}

static void
ResetInst (ObjPtr obj, int i)
{
    obj->used = i;
    while (obj->used_stat && ObjStat(obj, obj->used_stat - 1)->inst > i)
	obj->used_stat--;
}

#define NewInst(_o,_op,_i,_stat) \
{\
    InstPtr __inst__; \
    (_o) = AddInst(_o, _stat); \
    (_i) = ObjLast(_o); \
    __inst__ = ObjCode (_o, _i); \
    __inst__->base.opCode = (_op); \
    __inst__->base.flags = 0; \
}

#define BuildInst(_o,_op,_inst,_stat) \
{\
    (_o) = AddInst (_o, _stat); \
    (_inst) = ObjCode(_o, ObjLast(_o)); \
    (_inst)->base.opCode = (_op); \
    (_inst)->base.flags = 0; \
}

#define SetFlag(_o,_f) ((_o)->used ? (ObjCode((_o), \
					      ObjLast(_o))->base.flags |= (_f)) \
				     : 0)
#define SetPush(_o)	SetFlag(_o,InstPush)
#define SetAInit(_o)	SetFlag(_o,InstAInit)


/*
 * Select the correct code body depending on whether
 * we're compiling a static initializer
 */
#define CodeBody(c) ((c)->func.inStaticInit ? &(c)->func.staticInit : &(c)->func.body)

typedef enum _tail { TailNever, TailVoid, TailAlways } Tail;

ObjPtr	CompileLvalue (ObjPtr obj, ExprPtr expr, ExprPtr stat, CodePtr code, Bool createIfNecessary, Bool assign, Bool initialize);
ObjPtr	CompileBinOp (ObjPtr obj, ExprPtr expr, BinaryOp op, ExprPtr stat, CodePtr code);
ObjPtr	CompileBinFunc (ObjPtr obj, ExprPtr expr, BinaryFunc func, ExprPtr stat, CodePtr code, char *name);
ObjPtr	CompileUnOp (ObjPtr obj, ExprPtr expr, UnaryOp op, ExprPtr stat, CodePtr code);
ObjPtr	CompileUnFunc (ObjPtr obj, ExprPtr expr, UnaryFunc func, ExprPtr stat, CodePtr code, char *name);
ObjPtr	CompileAssign (ObjPtr obj, ExprPtr expr, Bool initialize, ExprPtr stat, CodePtr code);
ObjPtr	CompileAssignOp (ObjPtr obj, ExprPtr expr, BinaryOp op, ExprPtr stat, CodePtr code);
ObjPtr	CompileAssignFunc (ObjPtr obj, ExprPtr expr, BinaryFunc func, ExprPtr stat, CodePtr code, char *name);
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
static Bool CompileIsReachable (ObjPtr obj, int i);

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
	if (name->atom.atom == AtomId ("[]"))
	    CompileError (obj, stat, "Using [] outside of comprehension scope");
	else
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
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	obj = CompileArrayIndex (obj, expr->tree.right,
				 stat, code, &ndim);
	expr->base.type = TypeCombineArray (expr->tree.left->base.type,
					    ndim,
					    True);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible type, array '%T', for %d dimension operation",
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
	    CompileError (obj, stat, "Incompatible type, value '%T', for * operation",
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

static void
_CompileCheckException (ObjPtr obj, ExprPtr stat)
{
    SymbolPtr	except = CheckStandardException ();
    
    if (except)
	CompileError (obj, stat, "Exception \"%A\" raised during compilation",
		      except->symbol.name);
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
    int	    left, right;
    
    left = obj->used;
    obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
    SetPush (obj);
    right = obj->used;
    obj = _CompileExpr (obj, expr->tree.right, True, stat, code);
    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					 expr->base.tag,
					 expr->tree.right->base.type);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible types, left '%T', right '%T', for %O operation",
		      expr->tree.left->base.type,
		      expr->tree.right->base.type,
		      op);
	expr->base.type = typePoly;
    }
    else if (obj->used == left + 2 &&
	     ObjCode (obj, left)->base.opCode == OpConst &&
	     ObjCode (obj, right)->base.opCode == OpConst &&
	     !signalException)
    {
	inst = ObjCode (obj, left);
	inst->constant.constant = BinaryOperate (ObjCode(obj, left)->constant.constant,
						 ObjCode(obj, right)->constant.constant,
						 op);
	_CompileCheckException (obj, stat);
	inst->base.flags &= ~InstPush;
	obj->used = left + 1;
    }
    else
    {
	BuildInst (obj, OpBinOp, inst, stat);
	inst->binop.op = op;
    }
    RETURN (obj);
}

ObjPtr
CompileBinFunc (ObjPtr obj, ExprPtr expr, BinaryFunc func, ExprPtr stat, CodePtr code, char *name)
{
    ENTER ();
    InstPtr inst;
    int	    left, right;
    
    left = obj->used;
    obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
    SetPush (obj);
    right = obj->used;
    obj = _CompileExpr (obj, expr->tree.right, True, stat, code);
    expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					 expr->base.tag,
					 expr->tree.right->base.type);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible types, left '%T', right '%T', for %s operation",
		      expr->tree.left->base.type,
		      expr->tree.right->base.type,
		      name);
	expr->base.type = typePoly;
    }
    else if (obj->used == left + 2 &&
	     ObjCode (obj, left)->base.opCode == OpConst &&
	     ObjCode (obj, right)->base.opCode == OpConst &&
	     !signalException)
    {
	inst = ObjCode (obj, left);
	inst->constant.constant = (*func) (ObjCode(obj, left)->constant.constant,
					   ObjCode(obj, right)->constant.constant);
	_CompileCheckException (obj, stat);
	inst->base.flags &= ~InstPush;
	obj->used = left + 1;
    }
    else
    {
	BuildInst (obj, OpBinFunc, inst, stat);
	inst->binfunc.func = func;
    }
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
    int	    d;
    
    if (expr->tree.right)
	down = expr->tree.right;
    else
	down = expr->tree.left;
    d = obj->used;
    obj = _CompileExpr (obj, down, True, stat, code);
    expr->base.type = TypeCombineUnary (down->base.type, expr->base.tag);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible type, value '%T', for %U operation",
		      down->base.type, op);
	expr->base.type = typePoly;
    }
    else if (obj->used == d + 1 &&
	     ObjCode (obj, d)->base.opCode == OpConst &&
	     !signalException)
    {
	inst = ObjCode (obj, d);
	inst->constant.constant = UnaryOperate (ObjCode(obj, d)->constant.constant,
						op);
	_CompileCheckException (obj, stat);
	inst->base.flags &= ~InstPush;
	obj->used = d + 1;
    }
    else
    {
	BuildInst (obj, OpUnOp, inst, stat);
	inst->unop.op = op;
    }
    RETURN (obj);
}

ObjPtr
CompileUnFunc (ObjPtr obj, ExprPtr expr, UnaryFunc func, ExprPtr stat, CodePtr code, char *name)
{
    ENTER ();
    InstPtr inst;
    ExprPtr down;
    int	    d;
    
    if (expr->tree.right)
	down = expr->tree.right;
    else
	down = expr->tree.left;
    d = obj->used;
    obj = _CompileExpr (obj, down, True, stat, code);
    expr->base.type = TypeCombineUnary (down->base.type, expr->base.tag);
    if (!expr->base.type)
    {
	CompileError (obj, stat, "Incompatible type, value '%T', for %s operation",
		      down->base.type, name);
	expr->base.type = typePoly;
    }
    else if (obj->used == d + 1 &&
	     ObjCode (obj, d)->base.opCode == OpConst &&
	     !signalException)
    {
	inst = ObjCode (obj, d);
	inst->constant.constant = (*func) (ObjCode(obj, d)->constant.constant);
	_CompileCheckException (obj, stat);
	inst->base.flags &= ~InstPush;
	obj->used = d + 1;
    }
    else
    {
	BuildInst (obj, OpUnFunc, inst, stat);
	inst->unfunc.func = func;
    }
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
	CompileError (obj, stat, "Incompatible types, left '%T', right '%T', for = operation",
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
	CompileError (obj, stat, "Incompatible types, left '%T', right '%T', for %O= operation",
		      expr->tree.left->base.type,
		      expr->tree.right->base.type,
		      op);
	expr->base.type = typePoly;
    }
    BuildInst (obj, OpAssignOp, inst, stat);
    inst->binop.op = op;
    RETURN (obj);
}

ObjPtr
CompileAssignFunc (ObjPtr obj, ExprPtr expr, BinaryFunc func, ExprPtr stat, CodePtr code, char *name)
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
	CompileError (obj, stat, "Incompatible types, left '%T', right '%T', for %s= operation",
		      expr->tree.left->base.type,
		      expr->tree.right->base.type,
		      name);
	expr->base.type = typePoly;
    }
    BuildInst (obj, OpAssignFunc, inst, stat);
    inst->binfunc.func = func;
    RETURN (obj);
}

static ObjPtr
CompileArgs (ObjPtr obj, int *argcp, Bool *varactualp, ExprPtr arg, Bool pushValue, ExprPtr stat, CodePtr code)
{
    ENTER ();
    int	argc;
    
    argc = 0;
    *varactualp = False;
    while (arg)
    {
	if (pushValue)
	    SetPush (obj);
	if (arg->tree.left->base.tag == DOTS)
	{
	    InstPtr inst;
	    obj = _CompileExpr (obj, arg->tree.left->tree.left, True, stat, code);
	    BuildInst (obj, OpVarActual, inst, stat);
	    *varactualp = True;
	}
	else
	{
	    obj = _CompileExpr (obj, arg->tree.left, True, stat, code);
	}
	arg = arg->tree.right;
	pushValue = True;
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
    Type	*actual_type;
    int		i;
    Bool	varactual;
    
    func_type = TypeCombineFunction (type);
    if (!func_type)
    {
	CompileError (obj, stat, "Incompatible type, value '%T', for call",
		      type);
	EXIT ();
	return False;
    }
    
    if (func_type->base.tag == type_func)
    {
	argt = func_type->func.args;
	arg = args;
	i = 0;
	varactual = False;
	while ((arg && !varactual) || (argt && !argt->varargs))
	{
	    if (!argt)
	    {
		CompileError (obj, stat, "Too many parameters for function type '%T'", func_type);
		ret = False;
		break;
	    }
	    if (!arg)
	    {
		CompileError (obj, stat, "Too few parameters for function type '%T'", func_type);
		ret = False;
		break;
	    }
	    varactual = arg->tree.left->base.tag == DOTS;
	    if (varactual)
		actual_type = TypeCombineArray (arg->tree.left->tree.left->base.type, 1, False);
	    else
		actual_type = arg->tree.left->base.type;
	    if (!TypeCompatible (argt->type, actual_type, True))
	    {
		CompileError (obj, stat, "Incompatible types, formal '%T', actual '%T', for argument %d",
			      argt->type, arg->tree.left->base.type, i);
		ret = False;
	    }
	    i++;
	    if (!argt->varargs)
		argt = argt->next;
	    if (arg && (!varactual || !argt))
		arg = arg->tree.right;
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

DataType    NonLocalType = { MarkNonLocal, 0, "NonLocalType" };

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
 * + compile the code that generates a function object
 * + compile the args, pushing value on the stack
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
    Bool    varactual;

    obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
    obj = CompileArgs (obj, &argc, &varactual, expr->tree.right, True, stat, code);
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
	inst->ints.value = varactual ? -argc : argc;
    }
    else
    {
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = varactual ? -argc : argc;
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
    Bool	varactual;
    
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
    obj = CompileArgs (obj, &argc, &varactual, expr->tree.right, False, stat, code);
    if (!CompileTypecheckArgs (obj, sym->symbol.type, expr->tree.right, argc, stat))
	RETURN(obj);
    expr->base.type = typePoly;
    BuildInst (obj, OpRaise, inst, stat);
    inst->raise.argc = varactual ? -argc : argc;
    inst->raise.exception = sym;
    RETURN (obj);
}


/*
 * Compile a  twixt --
 *
 *  twixt (enter; leave) body
 *
 *  enter:
 *		enter
 *	OpEnterDone
 *	OpTwixt		enter: leave:
 *		body
 *	OpTwixtDone
 *  leave:
 *		leave
 *	OpLeaveDone
 *
 */

static ObjPtr
CompileTwixt (ObjPtr obj, ExprPtr expr, ExprPtr stat, CodePtr code)
{
    ENTER ();
    int	    enter_inst, twixt_inst;
    InstPtr inst;
    
    enter_inst = obj->used;

    /* Compile enter expression */
    if (expr->tree.left->tree.left)
	obj = _CompileExpr (obj, expr->tree.left->tree.left, True, stat, code);
    BuildInst (obj, OpEnterDone, inst, stat);
    
    /* here's where the twixt instruction goes */
    NewInst (obj, OpTwixt, twixt_inst, stat);

    /* Compile the body */
    obj = _CompileStat (obj, expr->tree.right->tree.left, False, code);
    
    BuildInst (obj, OpTwixtDone, inst, stat);
    
    /* finish the twixt instruction */
    inst = ObjCode (obj, twixt_inst);
    inst->twixt.enter = enter_inst - twixt_inst;
    inst->twixt.leave = obj->used - twixt_inst;

    /* Compile leave expression */
    if (expr->tree.left->tree.right)
	obj = _CompileExpr (obj, expr->tree.left->tree.right, False, stat, code);
    BuildInst (obj, OpLeaveDone, inst, stat);

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
	SetPush (obj);
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	if (!TypeCompatible (typePrim[rep_integer],
			     expr->tree.left->base.type,
			     True))
	{
	    CompileError (obj, stat, "Incompatible type, index '%T', for array index %d",
			  expr->tree.left->base.type, ndim);
	    break;
	}
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
CompileCountInitDimensions (TypePtr type, ExprPtr expr)
{
    int	    ndimMax, ndimSub, ndim;
    
    switch (expr->base.tag) {
    case ANONINIT:
	type = TypeCanon (type);	
	if (type->base.tag == type_struct)
	    ndim = 0;
	else
	    ndim = 1;
	break;
    case ARRAY:
	expr = expr->tree.left;
	ndimMax = 0;
	while (expr)
	{
	    if (expr->tree.left && expr->tree.left->base.tag != DOTS)
	    {
		ndimSub = CompileCountInitDimensions (type, expr->tree.left);
		if (ndimSub < 0)
		    return ndimSub;
		if (ndimMax && ndimSub != ndimMax)
		    return -1;
		ndimMax = ndimSub;
	    }
	    expr = expr->tree.right;
	}
	ndim = ndimMax + 1;
	break;
    default:
	ndim = 0;
	break;
    }
    return ndim;
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

static int
CompileCountImplicitDimensions (ExprPtr expr)
{
    switch (expr->base.tag) {
    case ARRAY:
	return 1 + CompileCountImplicitDimensions (expr->tree.left);
    case ANONINIT:
	return 0;
    case COMMA:
	return CompileCountImplicitDimensions (expr->tree.left);
    default:
	return 0;
    }
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
    
    if (!expr)
	dim = 0;
    else switch (expr->base.tag) {
    case ARRAY:
	dim = 0;
	expr = expr->tree.left;
	while (expr)
	{
	    if (expr->tree.left->base.tag == DOTS)
		return False;
	    if (ndims != 1)
	    {
		CompileSizeDimensions (expr->tree.left, dims + 1, ndims - 1);
		if (dims[1])
		    dim++;
	    }
	    else
		dim++;
	    expr = expr->tree.right;
	}
	break;
    case COMP:
	return False;
    case ANONINIT:
	dim = 0;
	break;
    default:
	dim = 1;
	if (expr->tree.left->base.tag == DOTS)
	    return False;
	if (ndims != 1)
	    CompileSizeDimensions (expr, dims + 1, ndims - 1);
	break;
    }
    if (dim > *dims)
	*dims = dim;
    return True;
}

static ExprPtr
CompileImplicitArray (ObjPtr obj, ExprPtr stat, ExprPtr inits, int ndim)
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
	RETURN (0);
    }
    sub = 0;
    for (n = 0; n < ndim; n++)
    {
	sub = NewExprTree (COMMA,
			   NewExprConst (TEN_NUM, NewInt (*dims++)),
			   sub);
    }
    RETURN(sub);
}

static ObjPtr
CompileArrayInit (ObjPtr obj, ExprPtr expr, Type *type, 
		  ExprPtr stat, CodePtr code);

static ObjPtr
CompileStructUnionInit (ObjPtr obj, ExprPtr expr, Type *type, 
			ExprPtr stat, CodePtr code);

static ExprPtr
CompileImplicitInit (Type *type);

static ObjPtr
CompileInit (ObjPtr obj, ExprPtr expr, Type *type,
	     ExprPtr stat, CodePtr code)
{
    ENTER ();

    type = TypeCanon (type);
    
    if (!expr || expr->base.tag == ANONINIT) 
    {
	switch (type->base.tag) {
	case type_array:
	    obj = CompileArrayInit (obj, 0, type, stat, code);
	    break;
	case type_struct:
	    obj = CompileStructUnionInit (obj, 0, type, stat, code);
	    break;
	case type_union:
	default:
	    CompileError (obj, stat, "Invalid empty initializer , type '%T'", type);
	    break;
	}
    }
    else switch (expr->base.tag) {
    case ARRAY:
    case COMP:
	if (type->base.tag != type_array)
	    CompileError (obj, stat, "Array initializer type mismatch, type '%T'",
			  type);
	else
	    obj = CompileArrayInit (obj, expr, type, stat, code);
	break;
    case STRUCT:
	if (type->base.tag != type_struct && type->base.tag != type_union)
	    CompileError (obj, stat, "Struct/union initializer type mismatch, type '%T'",
			  type);
	else
	    obj = CompileStructUnionInit (obj, expr, type, stat, code);
	break;
    default:
	obj = _CompileExpr (obj, expr, True, stat, code);
	if (!TypeCombineBinary (type, ASSIGN, expr->base.type))
	    CompileError (obj, stat, "Incompatible types, storage '%T', value '%T', for initializer",
		      type, expr->base.type);
    }
    RETURN (obj);
}

static ObjPtr
CompileArrayInits (ObjPtr obj, ExprPtr expr, TypePtr type, 
		   int ndim, ExprPtr stat, CodePtr code,
		   AInitMode mode)
{
    ENTER ();
    InstPtr	inst;
    ExprPtr	e;
    
    if (ndim == 0)
    {
	obj = CompileInit (obj, expr, type, stat, code);
    }
    else
    {
	ExprPtr next;

	switch (expr->base.tag) {
	case ARRAY:
	    for (e = expr->tree.left; e; e = next)
	    {
		AInitMode   subMode = AInitModeElement;
		
		next = e->tree.right;
		if (next && next->tree.left->base.tag == DOTS)
		{
		    subMode = AInitModeRepeat;
		    next = next->tree.right;
		}
		obj = CompileArrayInits (obj, e->tree.left, type,
					 ndim-1, stat, code, subMode);
	    }
	    break;
	case ANONINIT:
	    break;
	default:
	    CompileError (obj, stat, "Not enough initializer dimensions");
	    break;
	}
    }
    BuildInst (obj, OpInitArray, inst, stat);
    inst->ainit.dim = ndim;
    inst->ainit.mode = mode;
    RETURN (obj);
}

static ExprPtr
CompileArrayInitArgs (int ndim)
{
    ExprPtr a = NewExprConst (TEN_NUM, One);

    a->base.type = typePrim[rep_integer];
    if (!ndim)
	return 0;
    return NewExprTree (COMMA, a, CompileArrayInitArgs (ndim - 1));
}

static ArgType *
CompileComprehensionArgs (ExprPtr e)
{
    ArgType *down = 0;
    if (e->base.tag == COMMA)
    {
	down = CompileComprehensionArgs (e->tree.right);
	e = e->tree.left;
    }
    down = NewArgType (typePrim[rep_integer],
		       False, e->atom.atom,
		       e->atom.symbol, down);
    return down;
}

static ObjPtr
CompileComprehension (ObjPtr	obj,
		      TypePtr	type,
		      ExprPtr	expr,
		      ExprPtr	stat,
		      CodePtr	code)
{
    ENTER ();
    ExprPtr	body = expr->tree.right;
    ExprPtr	lambda;
    ArgType	*args;

    /*
     * Convert a single expression into a block containing a 
     * return statement
     */
    switch (body->base.tag) {
    case STRUCT:
    case COMP:
    case ARRAY:
    case ANONINIT:
	body = NewExprTree (NEW, body, 0);
	body->base.type = type;
    }
    if (body->base.tag != OC)
	body = NewExprTree (OC,
			    NewExprTree (RETURNTOK, 0, body),
			    NewExprTree (OC, 0, 0));
    /*
     * Convert the args
     */
    args = CompileComprehensionArgs (expr->tree.left->tree.left);
    /*
     * Compile [] symbol
     */
    CompileStorage (obj, stat, expr->tree.left->tree.right->atom.symbol,
		    code);
    /*
     * Build a func expression
     */
    lambda = NewExprCode (NewFuncCode (type,
				       args,
				       body),
			  0);
    obj = _CompileExpr (obj, lambda, True, stat, code);
    expr->tree.left->base.type = lambda->base.type;
    RETURN(obj);
}

static ObjPtr
CompileArrayInit (ObjPtr obj, ExprPtr expr, Type *type, ExprPtr stat, CodePtr code)
{
    ENTER ();
    int	    ndim;
    Type    *sub = type->array.type;
    Expr    *dimensions;

    ndim = CompileCountDeclDimensions (type->array.dimensions);
    if (!ndim)
    {
	if (expr)
	    ndim = CompileCountImplicitDimensions (expr);
	if (!ndim)
	{
	    CompileError (obj, stat, "Cannot compute number of array dimensions");
	    RETURN (obj);
	}
    }
    if (type->array.dimensions && type->array.dimensions->tree.left)
	dimensions = type->array.dimensions;
    else
    {
	dimensions = CompileImplicitArray (obj, stat, expr, ndim);
	if (!dimensions)
	    RETURN (obj);
    }
    if (expr && expr->base.tag == COMP)
    {
	ExprPtr	args = CompileArrayInitArgs (ndim);
	Type	*retType;
	
	obj = CompileComprehension (obj, sub, expr, stat, code);
	if (!CompileTypecheckArgs (obj, expr->tree.left->base.type,
				   args, ndim, stat))
	{
	    RETURN(obj);
	}
	retType = TypeCombineReturn (expr->tree.left->base.type);
	if (!TypeCombineBinary (sub, ASSIGN, retType))
	{
	    CompileError (obj, stat, "Incompatible types, array '%T', return '%T', for initializer",
			  sub, expr->base.type);
	    RETURN(obj);
	}
	SetPush (obj);
	obj = CompileLvalue (obj,
			     expr->tree.left->tree.right,
			     stat, code, False, True, True);
	SetPush (obj);
    }
    obj = CompileBuildArray (obj, expr, type, dimensions, ndim, stat, code);
    if (expr)
    {
	InstPtr	    inst;
	
	if (expr->base.tag == COMP)
	{
	    int	    start_inst;
	    int	    top_inst;
	    
	    /*
	     *	Comprehension:
	     *
	     *	    Obj		^ (obj)
	     *	    InitArray	  ndim (Start)
	     *	    Branch	  L1
	     * L2:  InitArray	  ndim (Func)
	     *	    Call
	     *	    InitArray	  0 (Element)
	     * L1:  InitArray	  n (Test)
	     *	    BranchFalse	  L2
	     *	    InitArray	  n (Element)
	     */
	    BuildInst (obj, OpAssign, inst, stat);
	    inst->assign.initialize = True;
	    BuildInst (obj, OpInitArray, inst, stat);
	    inst->ainit.mode = AInitModeStart;
	    inst->ainit.dim = ndim;
	    
	    /* Branch L1 */
	    NewInst (obj, OpBranch, start_inst, stat);

	    top_inst = obj->used;
	    BuildInst (obj, OpInitArray, inst, stat);
	    inst->ainit.dim = ndim;
	    inst->ainit.mode = AInitModeFunc;
	    
	    BuildInst (obj, OpCall, inst, stat);
	    inst->ints.value = ndim;
	    BuildInst (obj, OpInitArray, inst, stat);
	    inst->ainit.dim = 0;
	    inst->ainit.mode = AInitModeElement;
	    
	    /* Patch Branch L1 */
	    inst = ObjCode (obj, start_inst);
	    inst->branch.offset = obj->used - start_inst;
	    inst->branch.mod = BranchModNone;

	    BuildInst (obj, OpInitArray, inst, stat);
	    inst->ainit.dim = 0;
	    inst->ainit.mode = AInitModeTest;

	    /* Branch L2 */
	    BuildInst (obj, OpBranchFalse, inst, stat);
	    inst->branch.offset = top_inst - ObjLast(obj);
	    inst->branch.mod = BranchModNone;

	    /* Finish up */
	    BuildInst (obj, OpInitArray, inst, stat);
	    inst->ainit.dim = ndim;
	    inst->ainit.mode = AInitModeFuncDone;
	}
	else
	{
	    int	    ninitdim;
	    if (expr->base.tag != ARRAY && expr->base.tag != ANONINIT)
	    {
		CompileError (obj, stat, "Non array initializer");
		RETURN (obj);
	    }
	    ninitdim = CompileCountInitDimensions (sub, expr);
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
	    BuildInst (obj, OpInitArray, inst, stat);
	    inst->ainit.mode = AInitModeStart;
	    inst->ainit.dim = ndim;
	    obj = CompileArrayInits (obj, expr, sub, ndim, stat, code,
				     AInitModeElement);
	}
    }
    RETURN (obj);
}

static ExprPtr
CompileImplicitInit (Type *type)
{
    ENTER ();
    ExprPtr	    init = 0;
    Type	    *sub;
    int		    dim;
    StructTypePtr   structs;
    TypePtr	    *types;
    Atom	    *atoms;
    int		    i;
    
    type = TypeCanon (type);
    
    switch (type->base.tag) {
    case type_array:
	if (type->array.dimensions && type->array.dimensions->tree.left)
	{
	    sub = type->array.type;
	    init = CompileImplicitInit (sub);
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
	    else
		init = NewExprTree (ANONINIT, 0, 0);
	}
	break;
    case type_struct:
	structs = type->structs.structs;
	types = BoxTypesElements (structs->types);
	atoms = StructTypeAtoms (structs);
	init = 0;
	for (i = 0; i < structs->nelements; i++)
	{
	    ExprPtr	member;
	    
	    sub = TypeCanon (types[i]);

	    member = CompileImplicitInit (sub);
	    if (member)
	    {
		init = NewExprTree (COMMA,
				    NewExprTree (ASSIGN, 
						 NewExprAtom (atoms[i], 0, False), 
						 member),
				    init);
	    }
	}
	if (init)
	    init = NewExprTree (STRUCT, init, 0);
	else
	    init = NewExprTree (ANONINIT, 0, 0);
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
CompileStructUnionInit (ObjPtr obj, ExprPtr expr, Type *type,
			ExprPtr stat, CodePtr code)
{
    ENTER ();
    StructType	    *structs = type->structs.structs;
    InstPtr	    inst;
    ExprPtr	    inits = expr ? expr->tree.left : 0;
    ExprPtr	    init;
    Type	    *mem_type;
    int		    i;
    TypePtr	    *types = BoxTypesElements (structs->types);
    Atom	    *atoms = StructTypeAtoms (structs);
    
    if (type->base.tag == type_struct)
    {
	BuildInst (obj, OpBuildStruct, inst, stat);
	inst->structs.structs = structs;
	/*
	 * Initialize any elements which were given explicit values
	 */
	for (init = inits; init; init = init->tree.right)
	{
	    mem_type = StructMemType (structs, init->tree.left->tree.left->atom.atom);
	    if (!mem_type)
	    {
		CompileError (obj, stat, "Type '%T' is not a struct or union containing \"%A\"",
			      type, init->tree.left->tree.left->atom.atom);
		continue;
	    }
    
	    SetPush (obj);	/* push the struct */
	    
	    /* 
	     * Compute the initializer value 
	     */
	    obj = CompileInit (obj, init->tree.left->tree.right, mem_type, stat, code);
    
	    /*
	     * Assign to the member
	     */
	    BuildInst (obj, OpInitStruct, inst, stat);
	    inst->atom.atom = init->tree.left->tree.left->atom.atom;
	}
	
	/*
	 * Implicitly initialize any remaining elements
	 */
	for (i = 0; i < structs->nelements; i++)
	{
	    TypePtr	type = TypeCanon (types[i]);
    
	    if (!inits || !CompileStructInitElementIncluded (inits, atoms[i]))
	    {
		ExprPtr init = CompileImplicitInit (type);
    
		if (init)
		{
		    SetPush (obj);
		    obj = CompileInit (obj, init, type, stat, code);
		    BuildInst (obj, OpInitStruct, inst, stat);
		    inst->atom.atom = atoms[i];
		}
	    }
	}
    }
    else
    {
	init = inits;
	if (!init)
	{
	    CompileError (obj, stat, "Empty initializer for union '%T'",
			  type);
	    RETURN (obj);
	}
	if (init->tree.right)
	{
	    CompileError (obj, stat, "Multiple initializers for union '%T'",
			  type);
	    RETURN (obj);
	}
	
	mem_type = StructMemType (structs, init->tree.left->tree.left->atom.atom);
	if (!mem_type)
	{
	    CompileError (obj, stat, "Type '%T' is not a struct or union containing \"%A\"",
			  type, init->tree.left->tree.left->atom.atom);
	    RETURN (obj);
	}
	/* 
	 * Compute the initializer value 
	 */
	obj = CompileInit (obj, init->tree.left->tree.right, mem_type, stat, code);

	SetPush (obj);	    /* push the initializer value */

	BuildInst (obj, OpBuildUnion, inst, stat);
	inst->structs.structs = structs;

	BuildInst (obj, OpInitUnion, inst, stat);
	inst->atom.atom = init->tree.left->tree.left->atom.atom;
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
	    CompileError (obj, stat, "Incompatible types, formal '%T', actual '%T', for catch",
			  exception->symbol.type, catch_type);
	    RETURN (obj);
	}
	NewInst (obj, OpCatch, catch_inst, stat);

	/*
	 * Exception arguments are sitting in value, push
	 * them on the stack
	 */
	BuildInst (obj, OpNoop, inst, stat);
	SetPush (obj);

	/*
	 * Compile the exception handler and the
	 * call to get to it.
	 */
	catch->code.code->base.func = code ? code->base.func : 0;
	obj = CompileFunc (obj, catch->code.code, stat, code,
			   NewNonLocal (obj->nonLocal, 
					NonLocalCatch, 
					NON_LOCAL_RETURN));
	/*
	 * Patch non local returns inside
	 */
	CompilePatchLoop (obj, catch_inst, -1, -1);
	
	BuildInst (obj, OpExceptionCall, inst, stat);
	
	NewInst (obj, OpBranch, exception_inst, stat);
    
	inst = ObjCode (obj, catch_inst);
	inst->catch.offset = obj->used - catch_inst;
	inst->catch.exception = exception;
    
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalTry, 0);
	
	obj = CompileCatch (obj, catches->tree.right, body, stat, code);
	
	obj->nonLocal = obj->nonLocal->prev;

	BuildInst (obj, OpEndCatch, inst, stat);
	
	inst = ObjCode (obj, exception_inst);
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
    int	    top_inst, test_inst, middle_inst;
    InstPtr inst;
    SymbolPtr	s;
    Type	*t;
    OpCode	opCode;
    int		staticLink;
    Bool	bool_const;
    
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
	obj = CompileInit (obj, expr->tree.left, expr->base.type, stat, code);
	break;
    case UNION:
	if (expr->tree.right)
	    obj = _CompileExpr (obj, expr->tree.right, True, stat, code);
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
	    Type	*mt;
	    
	    expr->tree.left->base.type = StructMemType (st, expr->tree.left->atom.atom);
	    if (!expr->tree.left->base.type)
	    {
		CompileError (obj, stat, "Union type '%T' has no member \"%A\"",
			      expr->base.type,
			      expr->tree.left->atom.atom);
		break;
	    }
	    mt = TypeCanon (expr->tree.left->base.type);
	    BuildInst (obj, OpBuildUnion, inst, stat);
	    inst->structs.structs = st;
	    if (expr->tree.right)
	    {
		if (mt == typePrim[rep_void])
		{
		    CompileError (obj, stat, "Union type '%T', member '%A' requires no constructor value",
				  expr->base.type,
				  expr->tree.left->atom.atom);
		    break;
		}
		if (!TypeCombineBinary (expr->tree.left->base.type,
					ASSIGN,
					expr->tree.right->base.type))
		{
		    CompileError (obj, stat, "Incompatible types, member '%T', value '%T', for union constructor",
				  expr->tree.left->base.type,
				  expr->tree.right->base.type);
		    break;
		}
	    }
	    else 
	    {
		if (mt != typePrim[rep_void])
		{
		    CompileError (obj, stat, "Union member '%A' requires constructor value",
				  expr->tree.left->atom.atom);
		    break;
		}
	    }
	    BuildInst (obj, OpInitUnion, inst, stat);
	    inst->atom.atom = expr->tree.left->atom.atom;
	}
	else
	{
	    CompileError (obj, stat, "Incompatible type, type '%T', for union constructor", expr->base.type);
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
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	obj = CompileArrayIndex (obj, expr->tree.right,
				 stat, code, &ndim);
	expr->base.type = TypeCombineArray (expr->tree.left->base.type,
					    ndim,
					    False);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible type, array '%T', for %d dimension operation",
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
    case STAR:	    obj = CompileUnFunc (obj, expr, Dereference, stat, code,"*"); break;
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
    case LNOT:	    obj = CompileUnFunc (obj, expr, Lnot, stat, code,"~"); break;
    case BANG:	    obj = CompileUnFunc (obj, expr, Not, stat, code,"!"); break;
    case FACT:	    obj = CompileUnFunc (obj, expr, Factorial, stat, code,"!"); break;
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
	    CompileError (obj, stat, "Incompatible type, value '%T', for ++ operation ",
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
	    CompileError (obj, stat, "Incompatible type, value '%T', for -- operation",
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
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	SetPush (obj);
	obj = _CompileExpr (obj, expr->tree.right->tree.left, True, stat, code);
	SetPush (obj);
	obj = _CompileExpr (obj, expr->tree.right->tree.right, True, stat, code);
	expr->base.type = TypeCombineBinary (expr->tree.right->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types, left '%T', right '%T', for ** operation",
			  expr->tree.right->tree.left->base.type,
			  expr->tree.right->tree.right->base.type);
	    expr->base.type = typePoly;
	    break;
	}
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = 2;
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case SHIFTL:    obj = CompileBinFunc (obj, expr, ShiftL, stat, code, "<<"); break;
    case SHIFTR:    obj = CompileBinFunc (obj, expr, ShiftR, stat, code, ">>"); break;
    case QUEST:
	/*
	 * a ? b : c
	 *
	 * a QUEST b COLON c
	 *   +-------------+
	 *           +-------+
	 */
	top_inst = obj->used;
	obj = _CompileBoolExpr (obj, expr->tree.left, True, stat, code);
	if (obj->used == top_inst + 1 &&
	    (inst = ObjCode (obj, top_inst))->base.opCode == OpConst)
	{
	    test_inst = -1;
	    bool_const = True (inst->constant.constant);
	    obj->used = top_inst;
	}
	else
	{
	    NewInst (obj, OpBranchFalse, test_inst, stat);
	    bool_const = False;
	}
	top_inst = obj->used;
	obj = _CompileExpr (obj, expr->tree.right->tree.left, evaluate, stat, code);
	if (test_inst == -1)
	{
	    middle_inst = -1;
	    if (!bool_const)
		obj->used = top_inst;
	}
	else
	{
	    NewInst (obj, OpBranch, middle_inst, stat);
	    inst = ObjCode (obj, test_inst);
	    inst->branch.offset = obj->used - test_inst;
	    inst->branch.mod = BranchModNone;
	}
	
	top_inst = obj->used;
	obj = _CompileExpr (obj, expr->tree.right->tree.right, evaluate, stat, code);
	if (middle_inst == -1)
	{
	    if (bool_const)
		obj->used = top_inst;
	}
	else
	{
	    inst = ObjCode (obj, middle_inst);
	    inst->branch.offset = obj->used - middle_inst;
	    inst->branch.mod = BranchModNone;
	    BuildInst (obj, OpNoop, inst, stat);
	}
	
	expr->base.type = TypeCombineBinary (expr->tree.right->tree.left->base.type,
					     COLON,
					     expr->tree.right->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types, true '%T', false '%T', for ?: operation",
			  expr->tree.right->tree.left->base.type,
			  expr->tree.right->tree.right->base.type);
	    expr->base.type = typePoly;
	    break;
	}
	break;
    case LXOR:	    obj = CompileBinFunc (obj, expr, Lxor, stat, code, "^"); break;
    case LAND:	    obj = CompileBinOp (obj, expr, LandOp, stat, code); break;
    case LOR:	    obj = CompileBinOp (obj, expr, LorOp, stat, code); break;
    case AND:
	/*
	 * a && b
	 *
	 * a ANDAND b
	 *   +--------+
	 */
	top_inst = obj->used;
	obj = _CompileBoolExpr (obj, expr->tree.left, True, stat, code);
	if (obj->used == top_inst + 1 &&
	    (inst = ObjCode (obj, top_inst))->base.opCode == OpConst)
	{
	    test_inst = -1;
	    bool_const = True (inst->constant.constant);
	    if (bool_const)
		obj->used = top_inst;
	}
	else
	{
	    NewInst (obj, OpBranchFalse, test_inst, stat);
	    bool_const = True;
	}
	middle_inst = obj->used;
	/*
	 * Always compile the RHS to check for errors
	 */
	obj = _CompileBoolExpr (obj, expr->tree.right, evaluate, stat, code);
	/*
	 * Smash any instructions if they'll be skipped
	 */
	if (!bool_const)
	    obj->used = middle_inst;
	if (test_inst >= 0)
	{
	    inst = ObjCode (obj, test_inst);
	    inst->branch.offset = obj->used - test_inst;
	    inst->branch.mod = BranchModNone;
	    BuildInst (obj, OpNoop, inst, stat);
	}
	expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					     AND,
					     expr->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types, left '%T', right '%T', for && operation",
			  expr->tree.left->base.type,
			  expr->tree.right->base.type);
	    expr->base.type = typePoly;
	    break;
	}
	break;
    case OR:
	/*
	 * a || b
	 *
	 * a OROR b
	 *   +--------+
	 */
	top_inst = obj->used;
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	if (obj->used == top_inst + 1 &&
	    (inst = ObjCode (obj, top_inst))->base.opCode == OpConst)
	{
	    test_inst = -1;
	    bool_const = True (inst->constant.constant);
	    if (!bool_const)
		obj->used = top_inst;
	}
	else
	{
	    NewInst (obj, OpBranchTrue, test_inst, stat);
	    bool_const = False;
	}
	middle_inst = obj->used;
	/*
	 * Always compile the RHS to check for errors
	 */
	obj = _CompileExpr (obj, expr->tree.right, evaluate, stat, code);
	/*
	 * Smash any instructions if they'll be skipped
	 */
	if (bool_const)
	    obj->used = middle_inst;
	if (test_inst >= 0)
	{
	    inst = ObjCode (obj, test_inst);
	    inst->branch.offset = obj->used - test_inst;
	    inst->branch.mod = BranchModNone;
	    BuildInst (obj, OpNoop, inst, stat);
	}
	expr->base.type = TypeCombineBinary (expr->tree.left->base.type,
					     OR,
					     expr->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types, left '%T', right, '%T', for || operation",
			  expr->tree.left->base.type,
			  expr->tree.right->base.type);
	    expr->base.type = typePoly;
	    break;
	}
	break;
    case ASSIGN:	obj = CompileAssign (obj, expr, False, stat, code); break;
    case ASSIGNPLUS:	obj = CompileAssignOp (obj, expr, PlusOp, stat, code); break;
    case ASSIGNMINUS:	obj = CompileAssignOp (obj, expr, MinusOp, stat, code); break;
    case ASSIGNTIMES:	obj = CompileAssignOp (obj, expr, TimesOp, stat, code); break;
    case ASSIGNDIVIDE:	obj = CompileAssignOp (obj, expr, DivideOp, stat, code); break;
    case ASSIGNDIV:	obj = CompileAssignOp (obj, expr, DivOp, stat, code); break;
    case ASSIGNMOD:	obj = CompileAssignOp (obj, expr, ModOp, stat, code); break;
    case ASSIGNPOW:
	obj = _CompileExpr (obj, expr->tree.left, True, stat, code);
	SetPush (obj);
	obj = _CompileExpr (obj, expr->tree.right->tree.right, True, stat, code);
	SetPush (obj);
	obj = CompileLvalue (obj, expr->tree.right->tree.left, stat, code, False, False, False);
	expr->base.type = TypeCombineBinary (expr->tree.right->tree.left->base.type,
					     expr->base.tag,
					     expr->tree.right->tree.right->base.type);
	if (!expr->base.type)
	{
	    CompileError (obj, stat, "Incompatible types, left '%T', right '%T', for **= operation",
			  expr->tree.right->tree.left->base.type,
			  expr->tree.right->tree.right->base.type);
	    expr->base.type = typePoly;
	    break;
	}
	BuildInst (obj, OpCall, inst, stat);
	inst->ints.value = 2;
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case ASSIGNSHIFTL:	obj = CompileAssignFunc (obj, expr, ShiftL, stat, code, "<<"); break;
    case ASSIGNSHIFTR:	obj = CompileAssignFunc (obj, expr, ShiftR, stat, code, ">>"); break;
    case ASSIGNLXOR:	obj = CompileAssignFunc (obj, expr, Lxor, stat, code, "^"); break;
    case ASSIGNLAND:	obj = CompileAssignOp (obj, expr, LandOp, stat, code); break;
    case ASSIGNLOR:	obj = CompileAssignOp (obj, expr, LorOp, stat, code); break;
    case EQ:	    obj = CompileBinOp (obj, expr, EqualOp, stat, code); break;
    case NE:	    obj = CompileBinFunc (obj, expr, NotEqual, stat, code,"!="); break;
    case LT:	    obj = CompileBinOp (obj, expr, LessOp, stat, code); break;
    case GT:	    obj = CompileBinFunc (obj, expr, Greater, stat, code,">"); break;
    case LE:	    obj = CompileBinFunc (obj, expr, LessEqual, stat, code,"<="); break;
    case GE:	    obj = CompileBinFunc (obj, expr, GreaterEqual, stat, code,">="); break;
    case COMMA:	    
	top_inst = obj->used;
	obj = _CompileExpr (obj, expr->tree.left, False, stat, code);
	if (obj->used == top_inst + 1 &&
	    (inst = ObjCode (obj, top_inst))->base.opCode == OpConst)
	{
	    obj->used = top_inst;
	}
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
    case OC:
	/* statement block embedded in an expression */
	obj = _CompileStat (obj, expr, True, code);
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = Void;
	expr->base.type = typePrim[rep_void];
	break;
    default:
	assert(0);
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

static void
CompileMoveObj (ObjPtr	obj,
		int	start,
		int	depth,
		int	amount)
{
    InstPtr inst;

    while (start < obj->used)
    {
	inst = ObjCode (obj, start);
	switch (inst->base.opCode) {
	case OpFarJump:
	    if (inst->farJump.farJump->frame == depth &&
		inst->farJump.farJump->inst >= 0)
	    {
		inst->farJump.farJump->inst += amount;
	    }
	    break;
	case OpObj:
	    if (!inst->code.code->base.builtin &&
		inst->code.code->func.body.obj->nonLocal)
	    {
		if (inst->code.code->func.body.obj)
		    CompileMoveObj (inst->code.code->func.body.obj, 0,
				    depth + 1, amount);
		if (inst->code.code->func.staticInit.obj)
		    CompilePatchLoop (inst->code.code->func.staticInit.obj, 0,
				      depth + 1, amount);
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
    return !profiling && obj->nonLocal == 0 && (!code || code->base.func == code);
}

ObjPtr
_CompileBoolExpr (ObjPtr obj, ExprPtr expr, Bool evaluate, ExprPtr stat, CodePtr code)
{
    obj = _CompileExpr (obj, expr, evaluate, stat, code);
    if (!TypePoly (expr->base.type) && !TypeBool (expr->base.type))
    {
	CompileError (obj, expr, "Incompatible type, value '%T', for boolean", expr->base.type);
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
    StructType	*st;
    ObjPtr	cobj, bobj;
    
    switch (expr->base.tag) {
    case IF:
	/*
	 * if (a) b
	 *
	 * a BRANCHFALSE b
	 *   +-------------+
	 */
	top_inst = obj->used;
	obj = _CompileBoolExpr (obj, expr->tree.left, True, expr, code);
	if (obj->used == top_inst + 1 &&
	    (inst = ObjCode (obj, top_inst))->base.opCode == OpConst)
	{
	    Bool    t = True (inst->constant.constant);

	    obj->used = top_inst;
	    if (t)
		obj = _CompileStat (obj, expr->tree.right, last, code);
	}
	else
	{
	    NewInst (obj, OpBranchFalse, test_inst, expr);
	    obj = _CompileStat (obj, expr->tree.right, last, code);
	    inst = ObjCode (obj, test_inst);
	    inst->branch.offset = obj->used - test_inst;
	    inst->branch.mod = BranchModNone;
	}
	break;
    case ELSE:
	/*
	 * if (a) b else c
	 *
	 * a BRANCHFALSE b BRANCH c
	 *   +--------------------+
	 *                 +--------+
	 */
	top_inst = obj->used;
	obj = _CompileBoolExpr (obj, expr->tree.left, True, expr, code);
	if (obj->used == top_inst + 1 &&
	    (inst = ObjCode (obj, top_inst))->base.opCode == OpConst)
	{
	    Bool    t = True (inst->constant.constant);

	    obj->used = top_inst;
	    /*
	     * Check which side wins
	     */
	    if (t)
		obj = _CompileStat (obj, expr->tree.right->tree.left, last, code);
	    else
		obj = _CompileStat (obj, expr->tree.right->tree.right, last, code);
	}
	else
	{
	    NewInst (obj, OpBranchFalse, test_inst, expr);
	    /*
	     * Compile b
	     */
	    obj = _CompileStat (obj, expr->tree.right->tree.left, last, code);
	    /*
	     * Branch around else if reachable
	     */
	    if (CompileIsReachable (obj, obj->used))
	    {
		NewInst (obj, OpBranch, middle_inst, expr);
	    }
	    else
		middle_inst = -1;
	    /*
	     * Fix up branch on a
	     */
	    inst = ObjCode (obj, test_inst);
	    inst->branch.offset = obj->used - test_inst;
	    inst->branch.mod = BranchModNone;
	    /*
	     * Compile c
	     */
	    obj = _CompileStat (obj, expr->tree.right->tree.right, last, code);
	    /*
	     * Fix up branch around else if necessary
	     */
	    if (middle_inst != -1)
	    {
		inst = ObjCode (obj, middle_inst);
		inst->branch.offset = obj->used - middle_inst;
		inst->branch.mod = BranchModNone;
	    }
	}
	break;
    case WHILE:
	/*
	 * while (a) b
	 *
	 * a BRANCHFALSE b BRANCH
	 *   +--------------------+
	 * +---------------+
	 */
	
	cobj = NewObj (OBJ_INCR, OBJ_STAT_INCR);
	cobj = _CompileBoolExpr (cobj, expr->tree.left, True, expr, code);
	
	if (cobj->used == 1 &&
	    (inst = ObjCode (cobj, 0))->base.opCode == OpConst)
	{
	    Bool    t = True (inst->constant.constant);

	    if (!t)
	    {
		/* strip out the whole while loop */
		break;
	    }
	    start_inst = -1;
	}
	else
	{
	    NewInst (obj, OpBranch, start_inst, expr);
	}
	
        top_inst = obj->used;
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalControl,
				     NON_LOCAL_BREAK|NON_LOCAL_CONTINUE);
	obj = _CompileStat (obj, expr->tree.right, False, code);
	obj->nonLocal = obj->nonLocal->prev;

	continue_inst = obj->used;
	
	if (start_inst != -1)
	{
	    inst = ObjCode (obj, start_inst);
	    inst->branch.offset = obj->used - start_inst;
	    inst->branch.mod = BranchModNone;
	    
	    middle_inst = obj->used;
	    obj = AppendObj (obj, cobj);
	    CompileMoveObj (obj, middle_inst, 0, middle_inst);
	    BuildInst (obj, OpBranchTrue, inst, expr);
	    inst->branch.offset = top_inst - ObjLast(obj);
	    inst->branch.mod = BranchModNone;
	}
	else
	{
	    BuildInst (obj, OpBranch, inst, expr);
	    inst->branch.offset = top_inst - ObjLast(obj);
	    inst->branch.mod = BranchModNone;
	}
	
	CompilePatchLoop (obj, top_inst, continue_inst, obj->used);
	break;
    case DO:
	/*
	 * do a while (b);
	 *
	 * a b DO
	 * +---+
	 */
	top_inst = obj->used;
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalControl,
				     NON_LOCAL_BREAK|NON_LOCAL_CONTINUE);
	obj = _CompileStat (obj, expr->tree.left, False, code);
	obj->nonLocal = obj->nonLocal->prev;
	continue_inst = obj->used;
	obj = _CompileBoolExpr (obj, expr->tree.right, True, expr, code);
	if (obj->used == continue_inst + 1 &&
	    (inst = ObjCode (obj, continue_inst))->base.opCode == OpConst)
	{
	    Bool    t = True (inst->constant.constant);

	    obj->used = continue_inst;
	    if (t)
	    {
		BuildInst (obj, OpBranch, inst, expr);
		inst->branch.offset = top_inst - ObjLast(obj);
		inst->branch.mod = BranchModNone;
	    }
	}
	else
	{
	    BuildInst (obj, OpBranchTrue, inst, expr);
	    inst->branch.offset = top_inst - ObjLast(obj);
	    inst->branch.mod = BranchModNone;
	}
	CompilePatchLoop (obj, top_inst, continue_inst, obj->used);
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
	bobj = 0;
	if (expr->tree.left->tree.right)
	{
	    bobj = NewObj (OBJ_INCR, OBJ_STAT_INCR);
	    bobj = _CompileBoolExpr (bobj, 
				     expr->tree.left->tree.right,
				     True, expr, code);
	    NewInst (obj, OpBranch, start_inst, expr);
	}

	/* check for c */
	cobj = 0;
	if (expr->tree.right->tree.left)
	{
	    cobj = NewObj (OBJ_INCR, OBJ_STAT_INCR);
	    cobj = _CompileExpr (cobj, expr->tree.right->tree.left, False, expr, code);
	}
	    
	top_inst = obj->used;

	/* d */
	obj->nonLocal = NewNonLocal (obj->nonLocal, NonLocalControl,
				     NON_LOCAL_BREAK|NON_LOCAL_CONTINUE);
	obj = _CompileStat (obj, expr->tree.right->tree.right, False, code);
	obj->nonLocal = obj->nonLocal->prev;

	/* glue c into place */
	continue_inst = obj->used;
	if (cobj)
	{
	    middle_inst = obj->used;
	    obj = AppendObj (obj, cobj);
	    CompileMoveObj (obj, middle_inst, 0, middle_inst);
	}
	
	/* glue b into place */
	if (bobj)
	{
	    int	middle_inst = obj->used;
	    obj = AppendObj (obj, bobj);
	    CompileMoveObj (obj, middle_inst, 0, middle_inst);
	    if (obj->used == middle_inst + 1 &&
		(inst = ObjCode (obj, middle_inst))->base.opCode == OpConst)
	    {
		Bool	t = True(inst->constant.constant);

		obj->used = middle_inst;
		if (t)
		{
		    BuildInst (obj, OpBranch, inst, expr);
		    inst->branch.offset = top_inst - ObjLast (obj);
		    inst->branch.mod = BranchModNone;
		}
		else
		{
		    /* delete whole for body */
		    ResetInst (obj, start_inst);
		    continue_inst = top_inst = start_inst;
		}
	    }
	    else
	    {
		
		BuildInst (obj, OpBranchTrue, inst, expr);
		inst->branch.offset = top_inst - ObjLast(obj);
		inst->branch.mod = BranchModNone;
	    }
	    
	    /* patch start branch */
	    inst = ObjCode (obj, start_inst);
	    inst->branch.offset = middle_inst - start_inst;
	    inst->branch.mod = BranchModNone;
	}
	else
	{
	    BuildInst (obj, OpBranch, inst, expr);
	    inst->branch.offset = top_inst - ObjLast (obj);
	    inst->branch.mod = BranchModNone;
	}

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
	st = 0;
	if (expr->base.tag == SWITCH)
	    SetPush (obj);
	else
	{
	    if (expr->tree.left->base.type)
	    {
		Type	*t;
		
		t = TypeCanon (expr->tree.left->base.type);
		if (t->base.tag == type_union)
		    st = t->structs.structs;
		else if (!TypePoly (t))
		{
		    CompileError (obj, expr, "Union switch type '%T' not union",
				  expr->tree.left->base.type);
		}
	    }
	}
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
	/*
	 * Check to see if the union switch covers
	 * all possible values
	 */
	test_inst = 0;
	if (expr->base.tag == UNION)
	{
	    Bool	    missing = False;
	    int		    i;
	    Atom	    *atoms = StructTypeAtoms(st);
	    
	    /*
	     * See if every member of the union has a case
	     */
	    for (i = 0; i < st->nelements; i++)
	    {
		c = expr->tree.right;
		while (c)
		{
		    ExprPtr	pair = c->tree.left->tree.left;

		    if (pair)
		    {
			Atom	tag = pair->tree.left->atom.atom;
			if (tag == atoms[i])
			    break;
		    }
		    c = c->tree.right;
		}
		if (!c)
		{
		    missing = True;
		    break;
		}
	    }
	    if (!missing)
	    {
		test_inst = -1;
		if (has_default)
		    CompileError (obj, expr, "Union switch has unreachable default");
	    }
	    if (missing && !has_default)
		CompileError (obj, expr, "Union switch missing elements with no default");
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
		NewInst (obj, expr->base.tag == SWITCH ? OpCase : OpTagCase,
			 case_inst[icase], expr);
		icase++;
	    }
	    c = c->tree.right;
	}
	/* add default case at the bottom */
	if (test_inst == 0)
	{
	    /* don't know what the opcode is yet */
	    NewInst (obj, expr->base.tag == SWITCH ? OpDefault : OpBranch,
		     test_inst, expr);
	}
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
		    inst->branch.offset = obj->used - case_inst[icase];
		    inst->branch.mod = BranchModNone;
		}
		else
		{
		    ExprPtr	pair = c->tree.left->tree.left;
		    Atom	tag = pair->tree.left->atom.atom;
		    Type	*mt = typePoly;
		    
		    /*
		     * Find the member type
		     */
		    if (st)
		    {
			mt = StructMemType (st, tag);
			if (!mt)
			{
			    mt = typePoly;
			    CompileError (obj, expr, "Union case tag '%A' not in type '%T'",
					  tag, expr->tree.left->base.type);
			}
		    }
		    
		    /*
		     * Make sure there's no fall-through
		     */
		    if (icase > 0 && pair->tree.right &&
			CompileIsReachable (obj, obj->used))
		    {
			CompileError (obj, expr, 
				      "Fall through case with varient value");
		    }
		    inst->tagcase.offset = obj->used - case_inst[icase];
		    inst->tagcase.tag = tag;
		    /*
		     * this side holds the name to assign the
		     * switch value to.  Set it's type and
		     * build the assignment
		     */
		    if (pair->tree.right)
		    {
			SymbolPtr   name = pair->tree.right->atom.symbol;
			InstPtr	    assign;

			name->symbol.type = mt;
			CompileStorage (obj, expr, name, code);
			BuildInst (obj, OpTagStore, assign, expr);
			assign->var.name = name;
		    }
		}
		icase++;
	    }
	    else if (test_inst >= 0)
	    {
		inst = ObjCode (obj, test_inst);
		if (expr->base.tag == SWITCH)
		    inst->base.opCode = OpDefault;
		else
		    inst->base.opCode = OpBranch;
		inst->branch.offset = obj->used - test_inst;
		inst->branch.mod = BranchModNone;
		test_inst = -1;
	    }
	    while (s->tree.left)
	    {
		obj = _CompileStat (obj, s->tree.left, False, code);
		s = s->tree.right;
	    }
	    c = c->tree.right;
	}
	obj->nonLocal = obj->nonLocal->prev;
	/*
	 * Add a default branch if necessary
	 */
	if (test_inst >= 0)
	{
	    inst = ObjCode (obj, test_inst);
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
	    CompileError (obj, expr, "Incompatible types, formal '%T', actual '%T', for return",
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
CompileIsUnconditional (InstPtr inst)
{
    switch (inst->base.opCode) {
    case OpBranch:
    case OpFarJump:
    case OpDefault:
    case OpCatch:
    case OpReturn:
    case OpReturnVoid:
    case OpTailCall:
    case OpRaise:
	return True;
    default:
	return False;
    }
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
    case OpCatch:
	return True;
    default:
	return False;
    }
}

static Bool
CompileIsReachable (ObjPtr obj, int target)
{
    InstPtr inst;
    int	    i;

    for (i = 0; i < obj->used; i++)
    {
	inst = ObjCode (obj, i);
	if (CompileIsBranch (inst) && i + inst->branch.offset == target)
	    return True;
	if (i == target - 1 && !CompileIsUnconditional (inst))
	    return True;
    }
    return False;
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
    Bool    needReturn;
    
    code->base.previous = previous;
    obj = NewObj (OBJ_INCR, OBJ_STAT_INCR);
    obj->nonLocal = nonLocal;
    obj = _CompileStat (obj, code->func.code, True, code);
    needReturn = False;
    if (!obj->used || CompileIsReachable (obj, obj->used))
	needReturn = True;
    if (needReturn)
    {
	/*
	 * If control reaches the end of the function,
	 * flag an error for non-void functions,
	 * don't complain about void functions or catch blocks
	 */
	if (!nonLocal &&
	    !TypeCombineBinary (code->base.func->base.type, ASSIGN,
				typePrim[rep_void]))
	{
	    CompileError (obj, stat, "Control reaches end of function with type '%T'",
			  code->base.func->base.type);
	}
	BuildInst (obj, OpReturnVoid, inst, stat);
    }
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
	    init = CompileImplicitInit (s->symbol.type);
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
		    code_compile->func.staticInit.obj = NewObj (OBJ_INCR, OBJ_STAT_INCR);
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
	    *initObj = CompileInit (*initObj, init, s->symbol.type, stat, code);
	    if (code_compile)
	    {
		code_compile->func.inStaticInit = False;
		code->func.inGlobalInit = False;
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

    obj = NewObj (OBJ_INCR, OBJ_STAT_INCR);
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
    obj = NewObj (OBJ_INCR, OBJ_STAT_INCR);
    obj = _CompileExpr (obj, expr, True, stat, code);
    BuildInst (obj, OpEnd, inst, stat);
#ifdef DEBUG
    ObjDump (obj, 0);
#endif
    RETURN (obj);
}

const char *const OpNames[] = {
    "Noop",
    /*
     * Statement op codes
     */
    "Branch",
    "BranchFalse",
    "BranchTrue",
    "Case",
    "TagCase",
    "TagStore",
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
    "VarActual",
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
    static const struct {
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
    static const struct {
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
		"              " + strlen(OpNames[inst->base.opCode]),
		inst->base.flags & InstPush ? '^' : ' ');
    switch (inst->base.opCode) {
    case OpTagCase:
	FilePrintf (FileStdout, "(%A) ", inst->tagcase.tag);
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
	if (branch)
	{
	    j = i + inst->twixt.enter;
	    if (0 <= j && j < maxbranch)
		FilePrintf (FileStdout, "enter L%d", branch[j]);
	    else
		FilePrintf (FileStdout, "Broken enter %d", inst->branch.offset);
	    j = i + inst->twixt.leave;
	    if (0 <= j && j < maxbranch)
		FilePrintf (FileStdout, " leave L%d", branch[j]);
	    else
		FilePrintf (FileStdout, " Broken leave %d", inst->branch.offset);
	}
	else
	{
	    FilePrintf (FileStdout, "enter %d leave %d",
			inst->twixt.enter, inst->twixt.leave);
	}
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
    case OpTagStore:
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
	FilePrintf (FileStdout, "%d %s",
		    inst->ainit.dim,
		    inst->ainit.mode == AInitModeStart ? "start":
		    inst->ainit.mode == AInitModeElement ? "element":
		    inst->ainit.mode == AInitModeRepeat ? "repeat":
		    inst->ainit.mode == AInitModeFunc ? "func":
		    inst->ainit.mode == AInitModeTest ? "test": "?");
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
    FilePrintf (FileStdout, "%d instructions %d statements (0x%x)\n",
		obj->used, obj->used_stat, ObjCode(obj,0));
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
	if (inst->base.opCode == OpTwixt)
	{
	    j = i + inst->twixt.enter;
	    if (0 <= j && j < obj->used)
		if (!branch[j])
		    branch[j] = ++b;
	    j = i + inst->twixt.leave;
	    if (0 <= j && j < obj->used)
		if (!branch[j])
		    branch[j] = ++b;
	}
    }
    b = 0;
    stat = 0;
    for (i = 0; i < obj->used; i++)
    {
	ExprPtr	nextStat = ObjStatement (obj, inst = ObjCode (obj, i));
	if (nextStat && nextStat != stat)
	{
	    stat = nextStat;
	    FilePrintf (FileStdout, "                                     ");
	    PrettyStat (FileStdout, stat, False);
	}
	if (branch[i])
	    FilePrintf (FileStdout, "L%d:\n", branch[i]);
	InstDump (inst, indent, i, branch, obj->used);
    }
}

