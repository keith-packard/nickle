/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"value.h"

Value	Empty;

int
ArrayInit (void)
{
    ENTER ();
    Empty = NewArray (True, typesPoly, 0, 0);
    MemAddRoot (Empty);
    EXIT ();
    return 1;
}

Value
ArrayEqual (Value av, Value bv, int expandOk)
{
    Array   *a = &av->array, *b = &bv->array;
    int	    i;

    if (a->ndim != b->ndim)
	return Zero;
    for (i = 0; i < a->ndim; i++)
	if (a->dim[i] != b->dim[i])
	    return Zero;
    for (i = 0; i < a->ents; i++)
	if (Zerop (Equal ( BoxValue (a->values, i), 
			  BoxValue (b->values, i))))
	    return Zero;
    return One;
}

Bool
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
	for (i = 0; i < a->ndim; i++)
	{
	    FilePutInt (f, a->dim[i]);
	    if (i < a->ndim - 1)
		FilePuts (f, ",");
	}
	FilePuts (f, "] ");
	for (i = 0; i < a->ndim; i++)
	    FileOutput (f, '{');
    }
    for (i = 0; i < a->ents; )
    {
	if (!Print (f, BoxValue (a->values, i), format, base, width, prec, fill))
	{
	    ret = False;
	    break;
	}
	i++;
        ndone = 0;
	if (pretty)
	{
	    j = i;
	    k = a->ndim - 1;
	    while (k && j % a->dim[k] == 0)
	    {
		ndone++;
		j = j / a->dim[k];
		k--;
	    }
	    for (k = 0; k < ndone; k++)
		FileOutput (f, '}');
	}
	if (i < a->ents)
	{
	    if (pretty)
		FileOutput (f, ',');
	    FileOutput (f, ' ');
	    if (pretty)
		for (k = 0; k < ndone; k++)
		    FileOutput (f, '{');
	}
    }
    if (pretty)
	FilePuts (f, "}");
    EXIT ();
    return True;
}

void
ArrayMark (void *object)
{
    Array   *array = object;

    MemReference (array->type);
    MemReference (array->values);
}

ValueType    ArrayType = { 
    { ArrayMark, 0 },
    {
	0, 0, 0, 0, 0, 0,
	0, ArrayEqual, 0, 0,
    },
    {
	0,
    },
    0,
    0,
    ArrayPrint,
};

Value
NewArray (Bool constant, TypesPtr type, int ndim, int *dims)
{
    ENTER ();
    Value   ret;
    Value   def;
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
    ret = ALLOCATE (&ArrayType.data, sizeof (Array) + ndim * sizeof (int));
    ret->value.tag = type_array;
    ret->array.type = type;
    ret->array.ndim = ndim;
    ret->array.ents = ents;
    ret->array.dim = (int *) (&ret->array + 1);
    ret->array.values = NewBox (constant, ents);
    elements = BoxElements (ret->array.values);
    def = Default (type);
    for (i = 0; i < ents; i++)
    {
	elements[i].type = type;
	elements[i].value = def;
    }
    for (dim = 0; dim < ndim; dim++)
	ret->array.dim[dim] = dims[dim];
    RETURN (ret);
}
