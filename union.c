/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
    int		    i;

    for (i = 0; i < st->nelements; i++)
	if (StructTypeAtoms(st)[i] == name)
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
UnionPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    Union	    *u = &av->unions;

    if (format == 'v')
	FileOutput (f, '{');
    if (u->tag)
    {
	Type	*t = StructMemType (u->type, u->tag);
        FilePuts (f, AtomName (u->tag));
	if (t != (Type*) 1)
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
    
    if (!ValueIsUnion(av))
	return Equal (av, BoxValue (b->value, 0));
    if (!ValueIsUnion(bv))
	return Equal (BoxValue (a->value, 0), bv);
    if (a->tag != b->tag)
	return FalseVal;
    return Equal (BoxValue (a->value, 0), BoxValue (b->value, 0));
}

ValueRep    UnionRep = { 
    { UnionMark, 0, "UnionRep" },	    /* base */
    rep_union,		    /* tag */
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

    ret = ALLOCATE (&UnionRep.data, sizeof (Union));
    ret->unions.type = type;
    ret->unions.tag = 0;
    ret->unions.value = 0;
    ret->unions.value = NewBox (constant, False, 1, 0);
    RETURN (ret);
}

Type *
BuildUnionType (int nelements, ...)
{
    ENTER ();
    StructType	*st;
    int		i;
    char	*name;
    Type	*type;
    va_list	ap;

    st = NewStructType (nelements);
    va_start (ap, nelements);
    for (i = 0; i < nelements; i++)
    {
	type = va_arg (ap, Type *);
	name = va_arg (ap, char *);
	AddBoxType (&st->types, type);
	StructTypeAtoms (st)[i] = AtomId (name);
    }
    va_end (ap);
    RETURN (NewTypeUnion (st, False));
}

Type *
BuildEnumType (int nelements, ...)
{
    ENTER ();
    StructType	*st;
    int		i;
    char	*name;
    va_list	ap;

    st = NewStructType (nelements);
    va_start (ap, nelements);
    for (i = 0; i < nelements; i++)
    {
	name = va_arg (ap, char *);
	AddBoxType (&st->types, typePrim[rep_void]);
	StructTypeAtoms (st)[i] = AtomId (name);
    }
    va_end (ap);
    RETURN (NewTypeUnion (st, True));
}
