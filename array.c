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

static int
ArrayNextI (Array *a, int i)
{
    int	    step = 1;
    int	    sub_size = 1;
    int	    d;
    int	    dim, lim;
    int	    j = i + 1;

    for (d = 0; d < a->ndim - 1; d++)
    {
	dim = ArrayDims(a)[d];
	lim = ArrayLimits(a)[d];
	if (dim != lim && j % dim == lim)
	    step += (dim - lim) * sub_size;
	sub_size *= ArrayDims(a)[d];
	j /= dim;
    }
    return i + step;
}

static int
ArrayLimit (Value av)
{
    Array   *a = &av->array;
    int	    *limits = ArrayLimits(a);
    int	    *dims = ArrayDims(a);
    int	    d;
    int	    limit;

    limit = limits[a->ndim-1];
    for (d = 0; d < a->ndim - 1; d++)
	limit *= dims[d];
    return limit;
}

static Value
ArrayEqual (Value av, Value bv, int expandOk)
{
    Array   *a = &av->array, *b = &bv->array;
    int	    ai, bi;
    int	    alimit = ArrayLimit (av), blimit = ArrayLimit (bv);
    int	    d;

    if (a->ndim != b->ndim)
	return FalseVal;
    for (d = 0; d < a->ndim; d++)
	if (ArrayLimits(a)[d] != ArrayLimits(b)[d])
	    return FalseVal;
    ai = 0; bi = 0;
    while (ai < alimit && bi < blimit)
    {
	if (False (Equal ( BoxValue (a->values, ai), 
			  BoxValue (b->values, bi))))
	    return FalseVal;
	ai = ArrayNextI (a, ai);
	bi = ArrayNextI (b, bi);
    }
    return TrueVal;
}

static Bool
ArrayPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    ENTER ();
    Array   *a = &av->array;
    int	    *limits = ArrayLimits(a);
    int	    *dims = ArrayDims(a);
    int	    i, j, k;
    int	    ndone;
    int	    limit = ArrayLimit (av);
    Bool    ret = True;
    Bool    pretty = format == 'v' || format == 'g';

    if (pretty)
    {
	FilePuts (f, "[");
	for (i = a->ndim - 1; i >= 0; i--)
	{
	    FilePutInt (f, limits[i]);
	    if (i)
		FilePuts (f, ",");
	}
	FilePuts (f, "] ");
	for (i = 0; i < a->ndim; i++)
	    FileOutput (f, '{');
    }
    i = 0;
    while (i < limit)
    {
	if (!Print (f, BoxValueGet (a->values, i), format, base, width, prec, fill))
	{
	    ret = False;
	    break;
	}
	i = ArrayNextI (a, i);
	if (i < limit)
	{
	    ndone = 0;
	    if (pretty)
	    {
		j = i;
		k = 0;
		while (k < a->ndim - 1 && j % dims[k] == 0)
		{
		    ndone++;
		    j = j / dims[k];
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

#define MAX_HASH    1024

#define irot(i)	(((i) << 1) | ((i) >> (sizeof (int) * 8 - 1)))

static Value
ArrayHash (Value av)
{
    Array   *a = &av->array;
    int	    i;
    int	    h = 0;
    int	    limit = ArrayLimit (av);

    for (i = 0; i < limit; i = ArrayNextI (a, i))
	h = irot(h) ^ ValueInt (ValueHash (BoxValueGet (a->values, i)));
    return NewInt (h);
}

static void
ArrayMark (void *object)
{
    Array   *array = object;

    MemReference (array->values);
}

ValueRep    ArrayRep = { 
    { ArrayMark, 0, "ArrayRep" },
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
    0,
    ArrayHash,
};

Value
NewArray (Bool constant, TypePtr type, int ndim, int *dims)
{
    ENTER ();
    Value   ret;
    int	    ents;
    int	    dim;

    if (ndim)
    {
	ents = 1;
	for (dim = 0; dim < ndim; dim++)
	    ents *= dims[dim];
    }
    else
	ents = 0;
    ret = ALLOCATE (&ArrayRep.data, sizeof (Array) + (ndim * 2) * sizeof (int));
    ret->array.ndim = ndim;
    ret->array.ents = ents;
    ret->array.values = 0;
    ret->array.values = NewBox (constant, True, ents, type);
    for (dim = 0; dim < ndim; dim++)
	ArrayLimits(&ret->array)[dim] = ArrayDims(&ret->array)[dim] = dims[dim];
    RETURN (ret);
}

void
ArrayResize (Value av, int dim, int size)
{
    Array   *a = &av->array;
    int	    *dims = ArrayDims(a);
    int	    *limits = ArrayLimits(a);

    if (dims[dim] <= size)
    {
	ENTER ();
	BoxPtr	nbox;
	int	odim = dims[dim];
	int	ents = a->ents;
	int	chunk_size;
	int	chunk_skip;
	int	chunk_zero;
	int	d;
	int	nchunk;
	Value	*o, *n;

	if (!ents)
	{
	    chunk_size = 0;
	    for (d = 0; d < a->ndim; d++)
	    {
		dims[d] = 1;
		limits[d] = 1;
	    }
	    odim = 1;
	    ents = 1;
	    nchunk = 1;
	}
	else
	{
	    chunk_size = 1;
	    for (d = 0; d <= dim; d++)
		chunk_size *= dims[d];
	    nchunk = ents / chunk_size;
	}
	chunk_skip = chunk_size;
	while (odim < size)
	{
	    odim <<= 1;
	    ents <<= 1;
	    chunk_skip <<= 1;
	}
	nbox = NewBox (a->values->constant, True, ents, a->values->u.type);
	o = BoxElements (a->values);
	n = BoxElements (nbox);
	chunk_zero = chunk_skip - chunk_size;
	while (nchunk--)
	{
	    memcpy (n, o, chunk_size * sizeof (Value));
	    o += chunk_size;
	    n += chunk_size;
	    memset (n, '\0', chunk_zero * sizeof (Value));
	    n += chunk_zero;
	}
	a->values = nbox;
	a->ents = ents;
	dims[dim] = odim;
	EXIT ();
    }
    limits[dim] = size;
}
