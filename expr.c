/* $Header$ */
/*
 * This program is Copyright (C) 1988 by Keith Packard.  NICK is provided to
 * you without charge, and with no warranty.  You may give away copies of
 * NICK, including source, provided that this notice is included in all the
 * files.
 */

/*
 *	expr.c
 *
 *	handle expression trees
 */

# include	"nick.h"
# include	"y.tab.h"

static void
ExprTreeMark (void *object)
{
    ExprTree	*et = object;

    MemReference (et->left);
    MemReference (et->right);
}

static void
ExprConstMark (void *object)
{
    ExprConst	*ec = object;

    MemReference (ec->constant);
}

static void
ExprAtomMark (void *object)
{
}

static void
ExprCodeMark (void *object)
{
    ExprCode	*ec = object;

    MemReference (ec->code);
}

static void
ExprDeclMark (void *object)
{
    ExprDecl	*ed = object;

    MemReference (ed->decl);
}

DataType    ExprTreeType = { ExprTreeMark, 0 };
DataType    ExprConstType = { ExprConstMark, 0 };
DataType    ExprAtomType = { ExprAtomMark, 0 };
DataType    ExprCodeType = { ExprCodeMark, 0 };
DataType    ExprDeclType = { ExprDeclMark, 0 };

Expr *
NewExprTree(int tag, Expr *left, Expr *right)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprTreeType, sizeof (ExprTree));
    e->base.tag = tag;
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
    e->base.tag = CONST;
    e->constant.constant = val;
    RETURN (e);
}

Expr *
NewExprAtom (Atom atom)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprAtomType, sizeof (ExprAtom));
    e->base.tag = NAME;
    e->atom.atom = atom;
    RETURN (e);
}

Expr *
NewExprCode (CodePtr code, Atom name)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprCodeType, sizeof (ExprCode));
    e->base.tag = FUNCTION;
    e->code.code = code;
    code->base.name = name;
    RETURN (e);
}

Expr *
NewExprDecl (DeclListPtr decl, Class class, Type type)
{
    ENTER ();
    Expr    *e;

    e = ALLOCATE (&ExprDeclType, sizeof (ExprDecl));
    e->base.tag = VAR;
    e->decl.decl = decl;
    e->decl.class = class;
    e->decl.type = type;
    RETURN (e);
}
