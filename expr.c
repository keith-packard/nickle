/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	expr.c
 *
 *	handle expression trees
 */

#include	"nickle.h"
#include	"gram.h"

static void
ExprTreeMark (void *object)
{
    ExprTree	*et = object;

    MemReference (et->expr.namespace);
    MemReference (et->expr.type);
    MemReference (et->left);
    MemReference (et->right);
    if (!profiling)
	et->expr.ticks = et->expr.sub_ticks = 0;
}

static void
ExprConstMark (void *object)
{
    ExprConst	*ec = object;

    MemReference (ec->expr.namespace);
    MemReference (ec->expr.type);
    MemReference (ec->constant);
    if (!profiling)
	ec->expr.ticks = ec->expr.sub_ticks = 0;
}

static void
ExprAtomMark (void *object)
{
    ExprAtom	*ea = object;
    MemReference (ea->expr.namespace);
    MemReference (ea->expr.type);
    MemReference (ea->symbol);
    if (!profiling)
	ea->expr.ticks = ea->expr.sub_ticks = 0;
}

static void
ExprCodeMark (void *object)
{
    ExprCode	*ec = object;

    MemReference (ec->expr.namespace);
    MemReference (ec->expr.type);
    MemReference (ec->code);
    if (!profiling)
	ec->expr.ticks = ec->expr.sub_ticks = 0;
}

static void
ExprDeclMark (void *object)
{
    ExprDecl	*ed = object;

    MemReference (ed->expr.namespace);
    MemReference (ed->expr.type);
    MemReference (ed->decl);
    MemReference (ed->type);
    if (!profiling)
	ed->expr.ticks = ed->expr.sub_ticks = 0;
}

static void
ExprTypeMark (void *object)
{
    ExprType	*et = object;

    MemReference (et->expr.namespace);
    MemReference (et->expr.type);
    MemReference (et->left);
    MemReference (et->type);
    if (!profiling)
	et->expr.ticks = et->expr.sub_ticks = 0;
}

DataType    ExprTreeType = { ExprTreeMark, 0, "ExprTreeType" };
DataType    ExprConstType = { ExprConstMark, 0, "ExprConstType" };
DataType    ExprAtomType = { ExprAtomMark, 0, "ExprAtomType" };
DataType    ExprCodeType = { ExprCodeMark, 0, "ExprCodeType" };
DataType    ExprDeclType = { ExprDeclMark, 0, "ExprDeclType" };
DataType    ExprTypeType = { ExprTypeMark, 0, "ExprTypeType" };

static void
ExprBaseInit (Expr *e, int tag)
{
    e->base.tag = tag;
    e->base.file = LexFileName ();
    e->base.line = LexFileLine ();
    e->base.namespace = CurrentNamespace;
    e->base.type = 0;
    e->base.ticks = 0 ;
    e->base.sub_ticks = 0;
}

Expr *
NewExprTree(int tag, Expr *left, Expr *right)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprTreeType, sizeof (ExprTree));
    ExprBaseInit (e, tag);
    if (left)
    {
	if (left->base.file == e->base.file && left->base.line < e->base.line)
	    e->base.line = left->base.line;
    }
    else if (right)
    {
	if (right->base.file == e->base.file && right->base.line < e->base.line)
	    e->base.line = right->base.line;
    }
    e->tree.left = left;
    e->tree.right = right;
    RETURN ((Expr *) e);
}

Expr *
NewExprComma (Expr *left, Expr *right)
{
    return NewExprTree (COMMA, left, right);
}

Expr *
NewExprConst (int tag, Value val)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprConstType, sizeof (ExprConst));
    ExprBaseInit (e, tag);
    e->constant.constant = val;
    RETURN (e);
}

Expr *
NewExprAtom (Atom atom, SymbolPtr symbol, Bool privateFound)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprAtomType, sizeof (ExprAtom));
    ExprBaseInit (e, NAME);
    e->atom.atom = atom;
    e->atom.symbol = symbol;
    e->atom.privateFound = privateFound;
    RETURN (e);
}

Expr *
NewExprCode (CodePtr code, ExprPtr name)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprCodeType, sizeof (ExprCode));
    ExprBaseInit (e, FUNC);
    e->code.code = code;
    code->base.name = name;
    RETURN (e);
}

Expr *
NewExprDecl (int tag, DeclListPtr decl, Class class, Type *type, Publish publish)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprDeclType, sizeof (ExprDecl));
    ExprBaseInit (e, tag);
    e->decl.decl = decl;
    e->decl.class = class;
    e->decl.type = type;
    e->decl.publish = publish;
    RETURN (e);
}

Expr *
NewExprType (int tag, ExprPtr left, Type *type)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprTypeType, sizeof (ExprType));
    ExprBaseInit (e, tag);
    e->type.left = left;
    e->type.type = type;
    RETURN (e);
}

/*
 * LALR grammars like to build things right to left, but
 * sometimes we like the resulting data structure to be left to right
 */
Expr*
ExprRehang (Expr *e, Expr *right)
{
    if (e->tree.left)
    {
	Expr	*t, *left;

	left = e->tree.right;
	t = ExprRehang (e->tree.left, e);
	e->tree.left = left;
	e->tree.right = right;
	return t;
    }
    else
    {
	e->tree.left = e->tree.right;
	e->tree.right = right;
	return e;
    }
}
