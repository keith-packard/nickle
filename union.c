/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

Value
UnionRef (Value uv, Atom name)
{
    ENTER ();
    Union	    *u = &uv->unions;
    StructType	    *st = u->type;
    StructElement   *se = StructTypeElements(st);
    int		    i;

    for (i = 0; i < st->nelements; i++)
	if (se[i].name == name)
	{
	    u->tag = name;
	    RETURN (NewRef (u->value, 0));
	}
    RETURN (0);
}

Value
UnionValue (Value uv, Atom name)
{
    ENTER ();
    Union	    *u = &uv->unions;

    if (u->tag != name)
	RETURN (0);
    RETURN (BoxValue (u->value, 0));
}

static Bool
UnionPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    Union	    *u = &av->unions;

    if (format == 'v')
	FileOutput (f, '{');
    if (u->tag)
    {
	Types	*t = StructTypes (u->type, u->tag);
        FilePuts (f, AtomName (u->tag));
	if (t != (Types*) 1)
	{
	    FilePuts (f, " = ");
	    if (!Print (f, BoxValue (u->value, 0), format, base, width, prec, fill))
		return False;
	}
    }
    else
	FilePuts (f, "<unset>");
    if (format == 'v')
	FileOutput (f, '}');
    return True;
}

static void
UnionMark (void *object)
{
    Union  *u = object;

    MemReference (u->type);
    MemReference (u->value);
}

static Value
UnionEqual (Value av, Value bv, int expandOk)
{
    Union	    *a = &av->unions, *b = &bv->unions;
    
    if (av->value.tag != type_union)
	return Equal (av, BoxValue (b->value, 0));
    if (bv->value.tag != type_union)
	return Equal (BoxValue (a->value, 0), bv);
    if (a->tag != b->tag)
	return Zero;
    return Equal (BoxValue (a->value, 0), BoxValue (b->value, 0));
}

ValueType    unionType = { 
    { UnionMark, 0 },	    /* base */
    {			    /* binary */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	UnionEqual,
	0,
	0,
    },
    {			    /* unary */
	0,
	0,
	0,
    },
    0,
    0,
    UnionPrint,
    0,
};

Value
NewUnion (StructType *type, Bool constant)
{
    ENTER ();
    Value	    ret;

    ret = ALLOCATE (&unionType.data, sizeof (Union));
    ret->value.tag = type_union;
    ret->unions.type = type;
    ret->unions.tag = 0;
    ret->unions.value = NewBox (constant, False, 1);
    BoxType (ret->unions.value, 0) = 0;
    RETURN (ret);
}
