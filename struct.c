/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"value.h"

Value
StructRef (Value sv, Atom name)
{
    ENTER ();
    Struct	    *s = &sv->structs;
    StructType	    *st = s->type;
    StructElement   *se = StructTypeElements(st);
    int		    i;

    for (i = 0; i < st->nelements; i++)
	if (se[i].name == name)
	    RETURN (NewRef (s->values, i));
    RETURN (0);
}

Value
StructValue (Value sv, Atom name)
{
    ENTER ();
    Struct	    *s = &sv->structs;
    StructType	    *st = s->type;
    StructElement   *se = StructTypeElements(st);
    int		    i;

    for (i = 0; i < st->nelements; i++)
	if (se[i].name == name)
	    RETURN (BoxValue (s->values, i));
    RETURN (0);
}

Bool
StructPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    Struct	    *s = &av->structs;
    StructType	    *st = s->type;
    StructElement   *se = StructTypeElements(st);
    int		    i;

    if (format == 'v')
	FileOutput (f, '{');
    for (i = 0; i < st->nelements; i++)
    {
	FilePuts (f, AtomName (se[i].name));
	FilePuts (f, " = ");
	if (!Print (f, BoxValue (s->values, i), format, base, width, prec, fill))
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

ValueType    structType = { 
    { StructMark, 0 },	    /* base */
    {			    /* binary */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
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
};

Value
NewStruct (StructType *type, Bool constant)
{
    ENTER ();
    Value	    ret;
    int		    i;
    StructElement   *se;

    ret = ALLOCATE (&structType.data, sizeof (Struct));
    ret->value.tag = type_struct;
    ret->structs.type = type;
    ret->structs.values = NewBox (constant, type->nelements);
    se = StructTypeElements (type);    
    for (i = 0; i < type->nelements; i++)
    {
	BoxType (ret->structs.values, i) = se[i].type;
	BoxValue (ret->structs.values, i) = Default (se[i].type);
    }
    RETURN (ret);
}

static void
StructTypeMark (void *object)
{
}

DataType StructTypeType = { StructTypeMark, 0 };

StructType *
NewStructType (int nelements)
{
    ENTER ();
    StructType	    *st;
    int		    i;
    StructElement   *se;

    st = ALLOCATE (&StructTypeType, sizeof (StructType) + 
		   nelements * sizeof (StructElement));
    st->nelements = nelements;
    se = StructTypeElements (st);
    for (i = 0; i < nelements; i++)
    {
	se[i].type = type_undef;
	se[i].name = 0;
    }
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
