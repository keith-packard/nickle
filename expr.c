/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
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
extern Atom	yyfile;
extern int	yylineno;

static void
ExprTreeMark (void *object)
{
    ExprTree	*et = object;

    MemReference (et->expr.namespace);
    MemReference (et->expr.type);
    MemReference (et->left);
    MemReference (et->right);
}

static void
ExprConstMark (void *object)
{
    ExprConst	*ec = object;

    MemReference (ec->expr.namespace);
    MemReference (ec->expr.type);
    MemReference (ec->constant);
}

static void
ExprAtomMark (void *object)
{
    ExprAtom	*ea = object;
    MemReference (ea->expr.namespace);
    MemReference (ea->expr.type);
}

static void
ExprCodeMark (void *object)
{
    ExprCode	*ec = object;

    MemReference (ec->expr.namespace);
    MemReference (ec->expr.type);
    MemReference (ec->code);
}

static void
ExprDeclMark (void *object)
{
    ExprDecl	*ed = object;

    MemReference (ed->expr.namespace);
    MemReference (ed->expr.type);
    MemReference (ed->decl);
    MemReference (ed->type);
}

DataType    ExprTreeType = { ExprTreeMark, 0 };
DataType    ExprConstType = { ExprConstMark, 0 };
DataType    ExprAtomType = { ExprAtomMark, 0 };
DataType    ExprCodeType = { ExprCodeMark, 0 };
DataType    ExprDeclType = { ExprDeclMark, 0 };

static void
ExprBaseInit (Expr *e, int tag)
{
    e->base.tag = tag;
    e->base.file = yyfile;
    e->base.line = yylineno;
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
NewExprConst (Value val)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprConstType, sizeof (ExprConst));
    ExprBaseInit (e, CONST);
    e->constant.constant = val;
    RETURN (e);
}

Expr *
NewExprAtom (Atom atom)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprAtomType, sizeof (ExprAtom));
    ExprBaseInit (e, NAME);
    e->atom.atom = atom;
    RETURN (e);
}

Expr *
NewExprCode (CodePtr code, Atom name)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprCodeType, sizeof (ExprCode));
    ExprBaseInit (e, FUNCTION);
    e->code.code = code;
    code->base.name = name;
    RETURN (e);
}

Expr *
NewExprDecl (DeclListPtr decl, Class class, Types *type, Publish publish)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprDeclType, sizeof (ExprDecl));
    ExprBaseInit (e, VAR);
    e->decl.decl = decl;
    e->decl.class = class;
    e->decl.type = type;
    e->decl.publish = publish;
    RETURN (e);
}
