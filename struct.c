/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
    Bool	    pretty = format == 'v' || format == 'g' || format == 'G';
    char	    down_format = format == 'g' ? 'G' : format;

    if (pretty)
	FileOutput (f, '{');
    for (i = 0; i < st->nelements; i++)
    {
	FilePuts (f, AtomName (StructTypeAtoms(st)[i]));
	if (format != 'G') {
	    FilePuts (f, " = ");
	    if (!Print (f, BoxValueGet (s->values, i), down_format, base, width, prec, fill))
		return False;
	}
	if (i < st->nelements - 1)
	{
	    if (pretty)
		FileOutput (f, ',');
	    FileOutput (f, ' ');
	}
    }
    if (pretty)
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

static HashValue
StructHash (Value a)
{
    Struct	*s = &a->structs;
    StructType	*at = s->type;
    HashValue	h = 0;
    int		i;

    for (i = 0; i < at->nelements; i++)
	h = h ^ ValueInt (ValueHash (BoxValue (a->structs.values, i)));
    return h;
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

Type *
BuildStructType (int nelements, ...)
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
    RETURN (NewTypeStruct (st));
}
