/*
 * $Id$
 *
 * Copyright 1995 Network Computing Devices
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of NCD. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  NCD. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NCD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NCD.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, Network Computing Devices
 */

#include    "nick.h"
#include    "y.tab.h"

static void
TypeTreeMark (void *object)
{
    TypeTree	*tt = object;

    MemReference (tt->left);
    MemReference (tt->right);
}

static void
TypeNameMark (void *object)
{
}

static void
TypeExprMark (void *object)
{
    TypeExpr	*te = object;

    MemReference (te->expr);
}

DataType    TypeTreeType = { TypeTreeMark, 0 };
DataType    TypeNameType = { TypeNameMark, 0 };
DataType    TypeExprType = { TypeExprMark, 0 };

TypePtr
NewTypeTree (int tag, TypePtr left, TypePtr right)
{
    ENTER ();
    TypePtr t;

    t = ALLOCATE (&TypeTreeType, sizeof (TypeTree));
    t->base.tag = tag;
    t->tree.left = left;
    t->tree.right = right;
    RETURN (t);
}

TypePtr
NewTypeName (int tag, Atom name)
{
    ENTER ();
    TypePtr t;

    t = ALLOCATE (&TypeNameType, sizeof (TypeName));
    t->base.tag = tag;
    t->name.name = name;
    RETURN (t);
}

TypePtr
NewTypeExpr (int tag, ExprPtr expr)
{
    ENTER ();
    TypePtr t;

    t = ALLOCATE (&TypeExprType, sizeof (TypeExpr));
    t->base.tag = tag;
    t->tree.expr = expr;
    RETURN (t);
}
