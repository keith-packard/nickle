/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

Value	Empty;

int
ArrayInit (void)
{
    ENTER ();
    Empty = NewArray (True, typePoly, 0, 0);
    MemAddRoot (Empty);
    EXIT ();
    return 1;
}

static Value
ArrayEqual (Value av, Value bv, int expandOk)
{
    Array   *a = &av->array, *b = &bv->array;
    int	    i;

    if (a->ndim != b->ndim)
	return FalseVal;
    for (i = 0; i < a->ndim; i++)
	if (a->dim[i] != b->dim[i])
	    return FalseVal;
    for (i = 0; i < a->ents; i++)
	if (False (Equal ( BoxValue (a->values, i), 
			  BoxValue (b->values, i))))
	    return FalseVal;
    return TrueVal;
}

static Bool
ArrayPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    ENTER ();
    Array   *a = &av->array;
    int	    i, j, k;
    int	    ndone;
    Bool    ret = True;
    Bool    pretty = format == 'v' || format == 'g';

    if (pretty)
    {
	FilePuts (f, "[");
	for (i = a->ndim - 1; i >= 0; i--)
	{
	    FilePutInt (f, a->dim[i]);
	    if (i)
		FilePuts (f, ",");
	}
	FilePuts (f, "] ");
	for (i = 0; i < a->ndim; i++)
	    FileOutput (f, '{');
    }
    i = 0;
    while (i < a->ents)
    {
	if (!Print (f, BoxValueGet (a->values, i), format, base, width, prec, fill))
	{
	    ret = False;
	    break;
	}
	i++;
	if (i < a->ents)
	{
	    ndone = 0;
	    if (pretty)
	    {
		j = i;
		k = 0;
		while (k < a->ndim - 1 && j % a->dim[k] == 0)
		{
		    ndone++;
		    j = j / a->dim[k];
		    k++;
		}
		for (k = 0; k < ndone; k++)
		    FileOutput (f, '}');
		FileOutput (f, ',');
	    }
	    FileOutput (f, ' ');
	    if (pretty)
		for (k = 0; k < ndone; k++)
		    FileOutput (f, '{');
	}
    }
    if (pretty)
	for (i = 0; i < a->ndim; i++)
	    FileOutput (f, '}');
    EXIT ();
    return True;
}

static void
ArrayMark (void *object)
{
    Array   *array = object;

    MemReference (array->type);
    MemReference (array->values);
}

ValueRep    ArrayRep = { 
    { ArrayMark, 0 },
    rep_array,
    {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ArrayEqual,
	0,
	0,
    },
    {
	0,
    },
    0,
    0,
    ArrayPrint,
};

Value
NewArray (Bool constant, TypePtr type, int ndim, int *dims)
{
    ENTER ();
    Value   ret;
    int	    ents;
    int	    dim;
    BoxElement	*elements;
    int		i;

    if (ndim)
    {
	ents = 1;
	for (dim = 0; dim < ndim; dim++)
	    ents *= dims[dim];
    }
    else
	ents = 0;
    ret = ALLOCATE (&ArrayRep.data, sizeof (Array) + ndim * sizeof (int));
    ret->array.type = type;
    ret->array.ndim = ndim;
    ret->array.ents = ents;
    ret->array.dim = (int *) (&ret->array + 1);
    ret->array.values = 0;
    ret->array.values = NewBox (constant, True, ents);
    elements = BoxElements (ret->array.values);
    for (i = 0; i < ents; i++)
	elements[i].type = type;
    for (dim = 0; dim < ndim; dim++)
	ret->array.dim[dim] = dims[dim];
    RETURN (ret);
}
