/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

Value
StructMemRef (Value sv, Atom name)
{
    ENTER ();
    Struct	    *s = &sv->structs;
    StructType	    *st = s->type;
    int		    i;

    for (i = 0; i < st->nelements; i++)
	if (StructTypeAtoms(st)[i] == name)
	    RETURN (NewRef (s->values, i));
    RETURN (0);
}

Value
StructMemValue (Value sv, Atom name)
{
    ENTER ();
    Struct	    *s = &sv->structs;
    StructType	    *st = s->type;
    int		    i;

    for (i = 0; i < st->nelements; i++)
	if (StructTypeAtoms(st)[i] == name)
	    RETURN (BoxValue (s->values, i));
    RETURN (0);
}

Type *
StructMemType (StructType *st, Atom name)
{
    int		    i;

    for (i = 0; i < st->nelements; i++)
	if (StructTypeAtoms(st)[i] == name)
	    return (BoxTypesElements(st->types)[i]);
    return (0);
}

static Bool
StructPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    Struct	    *s = &av->structs;
    StructType	    *st = s->type;
    int		    i;

    if (format == 'v')
	FileOutput (f, '{');
    for (i = 0; i < st->nelements; i++)
    {
	FilePuts (f, AtomName (StructTypeAtoms(st)[i]));
	FilePuts (f, " = ");
	if (!Print (f, BoxValueGet (s->values, i), format, base, width, prec, fill))
	    return False;
	if (i < st->nelements - 1)
	{
	    if (format == 'v')
		FileOutput (f, ',');
	    FileOutput (f, ' ');
	}
    }
    if (format == 'v')
	FileOutput (f, '}');
    return True;
}

static void
StructMark (void *object)
{
    Struct  *s = object;

    MemReference (s->type);
    MemReference (s->values);
}

static Value
StructEqual (Value a, Value b, int expandOk)
{
    int		    i;
    StructType	    *at = a->structs.type;
    
    if (at->nelements != b->structs.type->nelements)
	return FalseVal;
    for (i = 0; i < at->nelements; i++)
    {
	if (False (Equal (BoxValue (a->structs.values, i),
			  StructMemValue (b, StructTypeAtoms(at)[i]))))
	    return FalseVal;
    }
    return TrueVal;
}

static Value
StructHash (Value a)
{
    Struct	*s = &a->structs;
    StructType	*at = s->type;
    int	    h = 0;
    int	    i;

    for (i = 0; i < at->nelements; i++)
	h = h ^ ValueInt (ValueHash (BoxValue (a->structs.values, i)));
    return NewInt (h);
}

ValueRep    StructRep = { 
    { StructMark, 0, "StructRep" },	    /* base */
    rep_struct,	    /* tag */
    {			    /* binary */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	StructEqual,
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
    StructPrint,
    0,
    StructHash,
};

Value
NewStruct (StructType *type, Bool constant)
{
    ENTER ();
    Value	    ret;

    ret = ALLOCATE (&StructRep.data, sizeof (Struct));
    ret->structs.type = type;
    ret->structs.values = 0;
    ret->structs.values = NewTypedBox (False, type->types);
    RETURN (ret);
}

static void
StructTypeMark (void *object)
{
    StructType	    *st = object;

    MemReference (st->types);
}

DataType StructTypeType = { StructTypeMark, 0, "StructTypeType" };

StructType *
NewStructType (int nelements)
{
    ENTER ();
    StructType	    *st;
    int		    i;
    Atom	    *atoms;

    st = ALLOCATE (&StructTypeType, sizeof (StructType) + 
		   nelements * sizeof (Atom));
    st->nelements = nelements;
    st->types = NewBoxTypes (nelements);
    atoms = StructTypeAtoms(st);
    for (i = 0; i < nelements; i++)
	atoms[i] = 0;
    RETURN (st);
}

Value	    Elementless;
StructType  *ElementlessType;

int
StructInit (void)
{
    ENTER ();

    ElementlessType = NewStructType (0);
    MemAddRoot (ElementlessType);
    Elementless = NewStruct (ElementlessType, True);
    MemAddRoot (Elementless);
    EXIT ();
    return 1;
}
