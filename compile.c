/*
 * $Id$
 *
 * Copyright 1996 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include    "nick.h"
#include    "y.tab.h"

#undef DEBUG

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

ObjPtr
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

ObjPtr
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

ObjPtr	_CompileLvalue (ObjPtr obj, ExprPtr expr, ScopePtr scope, ExprPtr stat);
ObjPtr	_CompileBinary (ObjPtr obj, ExprPtr expr, ScopePtr scope, OpCode opCode, ExprPtr stat);
ObjPtr	_CompileUnary (ObjPtr obj, ExprPtr expr, ScopePtr scope, OpCode opCode, ExprPtr stat);
ObjPtr	_CompileAssign (ObjPtr obj, ExprPtr expr, ScopePtr scope, OpCode opCode, ExprPtr stat);
ObjPtr	_CompileCall (ObjPtr obj, ExprPtr expr, ScopePtr scope, ExprPtr stat);
ObjPtr	_CompileArray (ObjPtr obj, ExprPtr expr, ScopePtr scope, OpCode opCode, ExprPtr stat);
ObjPtr	_CompileExpr (ObjPtr obj, ExprPtr expr, ScopePtr scope, ExprPtr stat);
void	_PatchLoop (ObjPtr obj, int start, int continue_offset);
ObjPtr	_CompileStat (ObjPtr obj, ExprPtr expr, ScopePtr scope);
ObjPtr	_CompileFunc (ObjPtr obj, CodePtr code, ScopePtr scope, ExprPtr stat);
ObjPtr	_CompileDecl (ObjPtr obj, ExprPtr decls, ScopePtr scope);
ObjPtr	_CompileFuncCode (CodePtr code, ScopePtr scope, ExprPtr stat);
void	CompileError (ObjPtr obj, ExprPtr stat, char *s, ...);

SymbolPtr
FindSymbol (ObjPtr obj, ExprPtr stat, ScopePtr scope, Atom name, int *depth, Type defType, Bool force)
{
    ENTER ();
    SymbolPtr   s;

    s = ScopeFindSymbol (scope, name, depth);
    if (!s)
    {
	if (scope->type != ScopeGlobal && !force)
	    CompileError (obj, stat, "No symbol \"%A\" in scope", name);
	switch (scope->type) {
	case ScopeGlobal:
	    s = NewSymbolGlobal (name, defType);
	    break;
	case ScopeFrame:
	    s = NewSymbolAuto (name, defType);
	    break;
	case ScopeStatic:
	    s = NewSymbolStatic (name, defType);
	    break;
	}
	*depth = 0;
	ScopeAddSymbol (scope, s);
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

ObjPtr
_CompileLvalue (ObjPtr obj, ExprPtr expr, ScopePtr scope, ExprPtr stat)
{
    ENTER ();
    InstPtr inst;
    
    switch (expr->base.tag) {
    case NAME:
	BuildInst(obj, OpNameRef, inst, stat);
	inst->var.name = FindSymbol (obj, stat, scope, expr->atom.atom, 
				     &inst->var.staticLink, type_undef, False);
	break;
    case DOT:
	obj = _CompileExpr (obj, expr->tree.left, scope, stat);
	BuildInst (obj, OpDotRef, inst, stat);
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case ARROW:
	obj = _CompileExpr (obj, expr->tree.left, scope, stat);
	BuildInst (obj, OpArrowRef, inst, stat);
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case OS:
	obj = _CompileArray (obj, expr, scope, OpArrayRef, stat);
	break;
    case STAR:
	obj = _CompileExpr (obj, expr->tree.left, scope, stat);
	break;
    }
    RETURN (obj);
}

ObjPtr
_CompileBinary (ObjPtr obj, ExprPtr expr, ScopePtr scope, OpCode opCode, ExprPtr stat)
{
    ENTER ();
    InstPtr inst;
    
    obj = _CompileExpr (obj, expr->tree.left, scope, stat);
    SetPush (obj);
    obj = _CompileExpr (obj, expr->tree.right, scope, stat);
    BuildInst (obj, opCode, inst, stat);
    RETURN (obj);
}

ObjPtr
_CompileUnary (ObjPtr obj, ExprPtr expr, ScopePtr scope, OpCode opCode, ExprPtr stat)
{
    ENTER ();
    InstPtr inst;
    
    if (expr->tree.right)
	obj = _CompileExpr (obj, expr->tree.right, scope, stat);
    else
	obj = _CompileExpr (obj, expr->tree.left, scope, stat);
    BuildInst (obj, opCode, inst, stat);
    RETURN (obj);
}

ObjPtr
_CompileAssign (ObjPtr obj, ExprPtr expr, ScopePtr scope, OpCode opCode, ExprPtr stat)
{
    ENTER ();
    InstPtr inst;
    
    obj = _CompileExpr (obj, expr->tree.right, scope, stat);
    SetPush (obj);
    obj = _CompileLvalue (obj, expr->tree.left, scope, stat);
    BuildInst (obj, opCode, inst, stat);
    RETURN (obj);
}

ObjPtr
_CompileCall (ObjPtr obj, ExprPtr expr, ScopePtr scope, ExprPtr stat)
{
    ENTER ();
    ExprPtr arg;
    InstPtr inst;
    int	    argc;

    arg = expr->tree.right;
    argc = 0;
    while (arg)
    {
	obj = _CompileExpr (obj, arg->tree.left, scope, stat);
	SetPush (obj);
	arg = arg->tree.right;
	argc++;
    }
    obj = _CompileExpr (obj, expr->tree.left, scope, stat);
    BuildInst (obj, OpCall, inst, stat);
    inst->ints.value = argc;
    BuildInst (obj, OpNoop, inst, stat);
    RETURN (obj);
}

ObjPtr
_CompileArray (ObjPtr obj, ExprPtr expr, ScopePtr scope, OpCode opCode, ExprPtr stat)
{
    ENTER ();
    ExprPtr	sub;
    InstPtr	inst;
    int		ndim;

    sub = expr->tree.right;
    ndim = 0;
    while (sub)
    {
	obj = _CompileExpr (obj, sub->tree.left, scope, stat);
	SetPush (obj);
	sub = sub->tree.right;
	ndim++;
    }
    obj = _CompileExpr (obj, expr->tree.left, scope, stat);
    BuildInst (obj, opCode, inst, stat);
    inst->ints.value = ndim;
    RETURN (obj);
}

ObjPtr
_CompileBuildArray (ObjPtr obj, ExprPtr expr, ScopePtr scope, ExprPtr stat)
{
    ENTER ();
    ExprPtr	sub;
    InstPtr	inst;
    int		ndim;

    sub = expr;
    ndim = 0;
    while (sub)
    {
	obj = _CompileExpr (obj, sub->tree.left, scope, stat);
	SetPush (obj);
	sub = sub->tree.right;
	ndim++;
    }
    BuildInst (obj, OpBuildArray, inst, stat);
    inst->ints.value = ndim;
    RETURN (obj);
}

int
_CompileCountDimensions (ExprPtr expr)
{
    int	    ndimMax, ndim;
    
    ndimMax = 0;
    if (expr->base.tag == AINIT)
    {
	expr = expr->tree.left;
	while (expr)
	{
	    ndim = _CompileCountDimensions (expr->tree.left) + 1;
	    if (ndim > ndimMax)
		ndimMax = ndim;
	    expr = expr->tree.right;
	}
    }
    return ndimMax;
}

void
_CompileSizeDimensions (ExprPtr expr, int *dims, int ndims)
{
    int	    dim;
    
    if (expr->base.tag == AINIT)
    {
	dim = 0;
	expr = expr->tree.left;
	while (expr)
	{
	    dim++;
	    if (ndims != 1)
		_CompileSizeDimensions (expr->tree.left, dims + 1, ndims - 1);
	    expr = expr->tree.right;
	}
    }
    else
    {
	dim = 1;
	if (ndims != 1)
	    _CompileSizeDimensions (expr, dims + 1, ndims - 1);
    }
    if (dim > *dims)
	*dims = dim;
}

ObjPtr
_CompileImplicitArray (ObjPtr obj, ExprPtr expr, ScopePtr scope, ExprPtr stat)
{
    ENTER ();
    ExprPtr sub;
    int	    ndim;
    int	    *dims;
    
    ndim = _CompileCountDimensions (expr);
    dims = AllocateTemp (ndim * sizeof (int));
    _CompileSizeDimensions (expr, dims, ndim);
    sub = 0;
    while (ndim--)
    {
	sub = NewExprTree (COMMA,
			   NewExprConst (NewInt (*dims++)),
			   sub);
    }
    obj = _CompileBuildArray (obj, sub, scope, stat);
    RETURN (obj);
}

ObjPtr
_CompileArrayInits (ObjPtr obj, ExprPtr expr, ScopePtr scope, int *ninits, ExprPtr stat)
{
    ENTER ();
    if (expr->base.tag == AINIT)
    {
	expr = expr->tree.left;
	while (expr)
	{
	    obj = _CompileArrayInits (obj, expr->tree.left, scope, ninits, stat);
	    expr = expr->tree.right;
	}
    }
    else
    {
	obj = _CompileExpr (obj, expr, scope, stat);
	SetPush (obj);
	++(*ninits);
    }
    RETURN (obj);
}

ObjPtr
_CompileStructInitValues (ObjPtr obj, ExprPtr expr, ScopePtr scope, ExprPtr stat)
{
    ENTER ();
    if (expr)
    {
	obj = _CompileStructInitValues (obj, expr->tree.right, scope, stat);
	obj = _CompileExpr (obj, expr->tree.left->tree.right, scope, stat);
	SetPush (obj);
    }
    RETURN (obj);
}

ObjPtr
_CompileStructInitAssigns (ObjPtr obj, ExprPtr expr, ScopePtr scope, ExprPtr stat)
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

ObjPtr
_CompileExpr (ObjPtr obj, ExprPtr expr, ScopePtr scope, ExprPtr stat)
{
    ENTER ();
    int	    i;
    int	    test_inst, middle_inst;
    InstPtr inst;
    
    switch (expr->base.tag) {
    case NAME:
	BuildInst (obj, OpName, inst, stat);
	inst->var.name = FindSymbol (obj, stat, scope, expr->atom.atom,
				     &inst->var.staticLink, type_undef, False);
	break;
    case ARRAY:
	if (expr->tree.right)
	{
	    i = 0;
	    obj = _CompileArrayInits (obj, expr->tree.right, scope, &i, stat);
	}
	if (expr->tree.left)
	{
	    obj = _CompileBuildArray (obj, expr->tree.left, scope, stat);
	}
	else
	{
	    obj = _CompileImplicitArray (obj, expr->tree.right, scope, stat);
	}
	if (expr->tree.right)
	{
	    BuildInst (obj, OpInitArray, inst, stat);
	    inst->ints.value = i;
	}
	break;
    case STRUCT:
	obj = _CompileStructInitValues (obj, expr->tree.right, scope, stat);
	BuildInst (obj, OpName, inst, stat);
	inst->var.name = FindSymbol (obj, stat, scope, 
				     expr->tree.left->atom.atom,
				     &inst->var.staticLink, type_struct, False);
	obj = _CompileStructInitAssigns (obj, expr->tree.right, scope, stat);
	break;
    case CONST:
	BuildInst (obj, OpConst, inst, stat);
	inst->constant.constant = expr->constant.constant;
	break;
    case OS:	    
	obj = _CompileArray (obj, expr, scope, OpArray, stat);
	break;
    case OP:	    /* function call */
	obj = _CompileCall (obj, expr, scope, stat);
	break;
    case DOT:
	obj = _CompileExpr (obj, expr->tree.left, scope, stat);
	BuildInst (obj, OpDot, inst, stat);
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case ARROW:
	obj = _CompileExpr (obj, expr->tree.left, scope, stat);
	BuildInst (obj, OpArrow, inst, stat);
	inst->atom.atom = expr->tree.right->atom.atom;
	break;
    case FUNCTION:
	obj = _CompileFunc (obj, expr->code.code, scope, stat);
	break;
    case STAR:	    obj = _CompileUnary (obj, expr, scope, OpStar, stat); break;
    case AMPER:	    obj = _CompileLvalue (obj, expr->tree.left, scope, stat); break;
    case UMINUS:    obj = _CompileUnary (obj, expr, scope, OpUminus, stat); break;
    case LNOT:	    obj = _CompileUnary (obj, expr, scope, OpLnot, stat); break;
    case BANG:	    obj = _CompileUnary (obj, expr, scope, OpBang, stat); break;
    case FACT:	    obj = _CompileUnary (obj, expr, scope, OpFact, stat); break;
    case INC:	    
	if (expr->tree.left)
	{
	    obj = _CompileLvalue (obj, expr->tree.left, scope, stat);
	    BuildInst (obj, OpPreInc, inst, stat);
	}
	else
	{
	    obj = _CompileLvalue (obj, expr->tree.right, scope, stat);
	    BuildInst (obj, OpPostInc, inst, stat);
	}
	break;
    case DEC:
	if (expr->tree.left)
	{
	    obj = _CompileLvalue (obj, expr->tree.left, scope, stat);
	    BuildInst (obj, OpPreDec, inst, stat);
	}
	else
	{
	    obj = _CompileLvalue (obj, expr->tree.right, scope, stat);
	    BuildInst (obj, OpPostDec, inst, stat);
	}
	break;
    case PLUS:	    obj = _CompileBinary (obj, expr, scope, OpPlus, stat); break;
    case MINUS:	    obj = _CompileBinary (obj, expr, scope, OpMinus, stat); break;
    case TIMES:	    obj = _CompileBinary (obj, expr, scope, OpTimes, stat); break;
    case DIVIDE:    obj = _CompileBinary (obj, expr, scope, OpDivide, stat); break;
    case DIV:	    obj = _CompileBinary (obj, expr, scope, OpDiv, stat); break;
    case MOD:	    obj = _CompileBinary (obj, expr, scope, OpMod, stat); break;
    case POW:	    obj = _CompileBinary (obj, expr, scope, OpPow, stat); break;
    case QUEST:
	/*
	 * a ? b : c
	 *
	 * a QUEST b COLON c
	 *   +-------------+
	 *           +-------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, scope, stat);
	NewInst (test_inst, obj);
	obj = _CompileExpr (obj, expr->tree.right->tree.left, scope, stat);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpQuest;
	inst->branch.offset = obj->used - test_inst;
	obj = _CompileExpr (obj, expr->tree.right->tree.right, scope, stat);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpColon;
	inst->branch.offset = obj->used - middle_inst;
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case LXOR:	    obj = _CompileBinary (obj, expr, scope, OpLxor, stat); break;
    case LAND:	    obj = _CompileBinary (obj, expr, scope, OpLand, stat); break;
    case LOR:	    obj = _CompileBinary (obj, expr, scope, OpLor, stat); break;
    case AND:
	/*
	 * a && b
	 *
	 * a ANDAND b
	 *   +--------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, scope, stat);
	NewInst (test_inst, obj);
	obj = _CompileExpr (obj, expr->tree.right, scope, stat);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpAnd;
	inst->base.stat = stat;
	inst->branch.offset = obj->used - test_inst;
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case OR:
	/*
	 * a || b
	 *
	 * a OROR b
	 *   +--------+
	 */
	obj = _CompileExpr (obj, expr->tree.left, scope, stat);
	NewInst (test_inst, obj);
	obj = _CompileExpr (obj, expr->tree.right, scope, stat);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpOr;
	inst->base.stat = stat;
	inst->branch.offset = obj->used - test_inst;
	BuildInst (obj, OpNoop, inst, stat);
	break;
    case ASSIGN:	obj = _CompileAssign (obj, expr, scope, OpAssign, stat); break;
    case ASSIGNPLUS:	obj = _CompileAssign (obj, expr, scope, OpAssignPlus, stat); break;
    case ASSIGNMINUS:	obj = _CompileAssign (obj, expr, scope, OpAssignMinus, stat); break;
    case ASSIGNTIMES:	obj = _CompileAssign (obj, expr, scope, OpAssignTimes, stat); break;
    case ASSIGNDIVIDE:	obj = _CompileAssign (obj, expr, scope, OpAssignDivide, stat); break;
    case ASSIGNDIV:	obj = _CompileAssign (obj, expr, scope, OpAssignDiv, stat); break;
    case ASSIGNMOD:	obj = _CompileAssign (obj, expr, scope, OpAssignMod, stat); break;
    case ASSIGNPOW:	obj = _CompileAssign (obj, expr, scope, OpAssignPow, stat); break;
    case ASSIGNLXOR:	obj = _CompileAssign (obj, expr, scope, OpAssignLxor, stat); break;
    case ASSIGNLAND:	obj = _CompileAssign (obj, expr, scope, OpAssignLand, stat); break;
    case ASSIGNLOR:	obj = _CompileAssign (obj, expr, scope, OpAssignLor, stat); break;
    case EQ:	    obj = _CompileBinary (obj, expr, scope, OpEq, stat); break;
    case NE:	    obj = _CompileBinary (obj, expr, scope, OpNe, stat); break;
    case LT:	    obj = _CompileBinary (obj, expr, scope, OpLt, stat); break;
    case GT:	    obj = _CompileBinary (obj, expr, scope, OpGt, stat); break;
    case LE:	    obj = _CompileBinary (obj, expr, scope, OpLe, stat); break;
    case GE:	    obj = _CompileBinary (obj, expr, scope, OpGe, stat); break;
    case COMMA:	    
	obj = _CompileExpr (obj, expr->tree.left, scope, stat);
	if (expr->tree.right)
	    obj = _CompileExpr (obj, expr->tree.right, scope, stat);
	break;
    case FORK:
	BuildInst (obj, OpFork, inst, stat);
	inst->obj.obj = CompileExpr (expr->tree.right, scope);
	break;
    case THREAD:
	obj = _CompileCall (obj, NewExprTree (OP, 
					      NewExprAtom (AtomId ("ThreadFromId")),
					      NewExprTree (COMMA, 
							   expr->tree.left,
							   (Expr *) 0)),
			    scope, stat);
	break;
    }
    RETURN (obj);
}

void
_PatchLoop (ObjPtr obj, int start, int continue_offset)
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

SymbolPtr
_MarkScope (ScopePtr scope, ScopeType type)
{
    while (scope && scope->type != type)
	scope = scope->previous;
    if (scope && scope->symbols)
	return scope->symbols->symbol;
    return 0;
}

void
_ResetScope (ScopePtr scope, ScopeType type, SymbolPtr mark)
{
    ScopeChainPtr   chain;
    CodePtr	    code;
    
    while (scope && scope->type != type)
	scope = scope->previous;
    if (scope)
    {
	code = 0;
	if (scope->type == ScopeFrame)
	    code = scope->code;
	while ((chain = scope->symbols))
	{
	    if (chain->symbol == mark)
		break;
	    scope->symbols = chain->next;
	    if (code)
	    {
		chain->next = code->func.locals->symbols;
		code->func.locals->symbols = chain;
	    }
	}
    }
}

ObjPtr
_CompileStat (ObjPtr obj, ExprPtr expr, ScopePtr scope)
{
    ENTER ();
    int		top_inst, continue_inst, test_inst, middle_inst, bottom_inst;
    InstPtr	inst;
    SymbolPtr	markFrame, markStatic, markGlobal;

    
    switch (expr->base.tag) {
    case IF:
	/*
	 * if (a) b
	 *
	 * a IF b
	 *   +----+
	 */
	obj = _CompileExpr (obj, expr->tree.left, scope, expr);
	NewInst (test_inst, obj);
	obj = _CompileStat (obj, expr->tree.right, scope);
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
	obj = _CompileExpr (obj, expr->tree.left, scope, expr);
	NewInst (test_inst, obj);
	obj = _CompileStat (obj, expr->tree.right->tree.left, scope);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpIf;
	inst->base.stat = expr;
	inst->branch.offset = obj->used - test_inst;
	obj = _CompileStat (obj, expr->tree.right->tree.right, scope);
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
	obj = _CompileExpr (obj, expr->tree.left, scope, expr);
	NewInst (test_inst, obj);
	obj = _CompileStat (obj, expr->tree.right, scope);
	NewInst (bottom_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpWhile;
	inst->base.stat = expr;
	inst->branch.offset = obj->used - test_inst;
	inst = ObjCode (obj, bottom_inst);
	inst->base.opCode = OpEndWhile;
	inst->base.stat = expr;
	inst->branch.offset = continue_inst - bottom_inst;
	_PatchLoop (obj, continue_inst, continue_inst);
	break;
    case DO:
	/*
	 * do a while (b);
	 *
	 * a b DO
	 * +---+
	 */
	continue_inst = obj->used;
	obj = _CompileStat (obj, expr->tree.left, scope);
	obj = _CompileExpr (obj, expr->tree.right, scope, expr);
	NewInst (test_inst, obj);
	inst = ObjCode (obj, test_inst);
	inst->base.opCode = OpDo;
	inst->base.stat = expr;
	inst->branch.offset = continue_inst - test_inst;
	_PatchLoop (obj, continue_inst, continue_inst);
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
	    obj = _CompileExpr (obj, expr->tree.left->tree.left, scope, expr);
	top_inst = obj->used;
	test_inst = -1;
	if (expr->tree.left->tree.right)
	{
	    obj = _CompileExpr (obj, expr->tree.left->tree.right, scope, expr);
	    NewInst (test_inst, obj);
	}
	obj = _CompileStat (obj, expr->tree.right->tree.right, scope);
	continue_inst = obj->used;
	if (expr->tree.right->tree.left)
	    obj = _CompileExpr (obj, expr->tree.right->tree.left, scope, expr);
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
	_PatchLoop (obj, top_inst, continue_inst);
	break;
    case OC:
	markFrame = _MarkScope (scope, ScopeFrame);
	markStatic = _MarkScope (scope, ScopeStatic);
	markGlobal = _MarkScope (scope, ScopeGlobal);
	while (expr->tree.left)
	{
	    obj = _CompileStat (obj, expr->tree.left, scope);
	    expr = expr->tree.right;
	}
	_ResetScope (scope, ScopeFrame, markFrame);
	_ResetScope (scope, ScopeStatic, markStatic);
	_ResetScope (scope, ScopeGlobal, markGlobal);
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
	    obj = _CompileExpr (obj, expr->tree.right, scope, expr);
	NewInst (middle_inst, obj);
	inst = ObjCode (obj, middle_inst);
	inst->base.opCode = OpReturn;
	inst->base.stat = expr;
	break;
    case EXPR:
	obj = _CompileExpr (obj, expr->tree.left, scope, expr);
	break;
    case SEMI:
	break;
    case FUNCTION:
	/*
	 * Make sure the symbol is defined in the current scope
	 */
	FindSymbol (obj, expr, scope, expr->tree.left->atom.atom,
		    &top_inst, type_func, True);
	obj = _CompileAssign (obj, expr, scope, OpAssign, expr);
	break;
    case VAR:
	obj = _CompileDecl (obj, expr, scope);
	break;
    }
    RETURN (obj);
}

ObjPtr
_CompileFuncCode (CodePtr code, ScopePtr scope, ExprPtr stat)
{
    ENTER ();
    ObjPtr  obj;
    InstPtr inst;
    
    obj = NewObj (OBJ_INCR);
    obj = _CompileStat (obj, code->func.code, scope);
    if (!obj->used || ObjCode (obj, ObjLast(obj))->base.opCode != OpReturn)
	BuildInst (obj, OpReturn, inst, stat);
#ifdef DEBUG
    ObjDump (obj);
#endif
    RETURN (obj);
}

ObjPtr
_CompileFunc (ObjPtr obj, CodePtr code, ScopePtr scope, ExprPtr stat)
{
    ENTER ();
    InstPtr	    inst;
    SymbolPtr	    local;
    ScopePtr	    statics;
    ScopePtr	    frames;
    ExprPtr	    args;
    DeclListPtr	    argd;
    ObjPtr	    staticInit;

    code->func.locals = NewScope (scope, ScopeFrame);
    statics = NewScope (scope, ScopeStatic);
    statics->code = code;
    frames = NewScope (statics, ScopeFrame);
    frames->code = code;
    for (args = code->func.args; args; args = args->tree.right)
    {
	for (argd = args->tree.left->decl.decl; argd; argd = argd->next)
	{
	    local = NewSymbolArg (argd->name, 
				  args->tree.left->decl.type);
	    ScopeAddSymbol (frames, local);
	}
    }
    code->base.argc = frames->count;
    code->func.obj = _CompileFuncCode (code, frames, stat);
    code->func.autoc = frames->count - code->base.argc;
    code->func.staticc = statics->count;
    /*
     * Empty the scope, moving all remaining variables into the code
     * structure
     */
    _ResetScope (statics, ScopeStatic, 0);
    _ResetScope (frames, ScopeFrame, 0);
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
_CompileDecl (ObjPtr obj, ExprPtr decls, ScopePtr scope)
{
    ENTER ();
    SymbolPtr	    s;
    DeclListPtr	    decl;
    Class	    class;
    ScopePtr	    compile_scope;
    ScopePtr	    lifetime_scope;
    ObjPtr	    *initObj;
    
    class = decls->decl.class;
    if (class == class_undef)
    {
	switch (scope->type) {
	case ScopeGlobal:
	    class = class_global;
	    break;
	case ScopeStatic:
	    class = class_static;
	    break;
	case ScopeFrame:
	    class = class_auto;
	    break;
	}
    }
    lifetime_scope = scope;
    compile_scope = 0;
    switch (class) {
    case class_global:
	/*
	 * Globals are compiled in the static initializer for
	 * the outermost enclosing function.
	 */
	while (lifetime_scope && lifetime_scope->type != ScopeGlobal)
	{
	    compile_scope = lifetime_scope;
	    lifetime_scope = lifetime_scope->previous;
	}
	break;
    case class_static:
	/*
	 * Statics are compiled in the static initializer for
	 * the enclosing static scope
	 */
	while (lifetime_scope && lifetime_scope->type != ScopeStatic)
	    lifetime_scope = lifetime_scope->previous;
	compile_scope = lifetime_scope;
	break;
    case class_auto:
    case class_arg:
	while (lifetime_scope && lifetime_scope->type != ScopeFrame)
	    lifetime_scope = lifetime_scope->previous;
	break;
    default:
	break;
    }
    if (!lifetime_scope)
    {
	CompileError (obj, decls, "Invalid storage class %C", class);
	RETURN (obj);
    }
    for (decl = decls->decl.decl; decl; decl = decl->next) {
	switch (class) {
	case class_global:
	    s = NewSymbolGlobal (decl->name, decls->decl.type);
	    break;
	case class_static:
	    s = NewSymbolStatic (decl->name, decls->decl.type);
	    break;
	case class_auto:
	default:
	    s = NewSymbolAuto (decl->name, decls->decl.type);
	    break;
	}
	/*
	 * Compile initializers in two steps
	 * compile the expression before placing the
	 * name in scope, and then compile the assignment
	 */
    	initObj = &obj;
	if (decl->init)
	{
	    if (compile_scope && compile_scope->code)
	    {
		CodePtr	code = compile_scope->code;
		if (!code->func.staticInit)
		    code->func.staticInit = NewObj (OBJ_INCR);
		initObj = &code->func.staticInit;
	    }
	    *initObj = _CompileExpr (*initObj, decl->init, 
				     lifetime_scope, decls);
	    SetPush (*initObj);
	}
	ScopeAddSymbol (scope, s);
	if (lifetime_scope != scope)
	    ScopeAddSymbol (lifetime_scope, s);
	if (decl->init)
	{
	    InstPtr inst;
	    
	    *initObj = _CompileLvalue (*initObj, NewExprAtom (decl->name),
				       lifetime_scope, decls);
	    BuildInst (*initObj, OpAssign, inst, decls);
	}
    }
    RETURN (obj);
}

ObjPtr
CompileStat (ExprPtr expr, ScopePtr scope)
{
    ENTER ();
    ObjPtr  obj;
    InstPtr inst;

    obj = NewObj (OBJ_INCR);
    obj = _CompileStat (obj, expr, scope);
    BuildInst (obj, OpEnd, inst, expr);
#ifdef DEBUG
    ObjDump (obj);
#endif
    RETURN (obj);
}

ObjPtr
CompileExpr (ExprPtr expr, ScopePtr scope)
{
    ENTER ();
    ObjPtr  obj;
    InstPtr inst;
    ExprPtr stat;

    stat = NewExprTree (EXPR, expr, 0);
    obj = NewObj (OBJ_INCR);
    obj = _CompileExpr (obj, expr, scope, stat);
    BuildInst (obj, OpEnd, inst, stat);
#ifdef DEBUG
    ObjDump (obj);
#endif
    RETURN (obj);
}

#ifdef DEBUG
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
    /*
     * Expr op codes
     */
    "Name",
    "NameRef",
    "Const",
    "BuildArray",
    "InitArray",
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
#endif

