/* $Header$ */

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

int
ArrayInit (void)
{
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
	FilePuts (f, "(");
	if (!TypePoly (ArrayType(a)))
	{
	    FilePutBaseType (f, ArrayType (a), False);
	    FilePuts (f, " ");
	}
	FilePuts (f, "[");
	for (i = a->ndim - 1; i >= 0; i--)
	{
	    if (a->resizable)
		FilePuts (f, "...");
	    else
		FilePutInt (f, limits[i]);
	    if (i)
		FilePuts (f, ", ");
	}
	FilePuts (f, "]");
	if (!TypePoly (ArrayType(a)))
	{
	    FilePutSubscriptType (f, ArrayType (a), False);
	}
	FilePuts (f, ") ");
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

#define hrot(i)	(((i) << 1) | ((i) >> (sizeof (HashValue) * 8 - 1)))

static HashValue
ArrayHash (Value av)
{
    Array	*a = &av->array;
    int		i;
    HashValue	h = 0;
    int		limit = ArrayLimit (av);

    for (i = 0; i < limit; i = ArrayNextI (a, i))
	h = hrot(h) ^ ValueInt (ValueHash (BoxValueGet (a->values, i)));
    return h;
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
NewArray (Bool constant, Bool resizable, TypePtr type, int ndim, int *dims)
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
    ret->array.resizable = resizable;
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

    assert (av->array.resizable);
    if (dims[dim] < size || dims[dim] > size * 2)
    {
	ENTER ();
	BoxPtr	obox = a->values;
	BoxPtr	nbox;
	int	odim = dims[dim];
	int	ents = obox->nvalues;
	int	chunk_size;
	int	chunk_ostride;
	int	chunk_nstride;
	int	chunk_zero;
	int	d;
	int	nchunk;
	Value	*o, *n;

	if (!ents)
	{
	    chunk_ostride = 0;
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
	    chunk_ostride = 1;
	    for (d = 0; d <= dim; d++)
		chunk_ostride *= dims[d];
	    nchunk = ents / chunk_ostride;
	}
	chunk_nstride = chunk_ostride;
	if (odim < size)
	{
	    /* bigger */
	    while (odim < size)
	    {
		odim <<= 1;
		ents <<= 1;
		chunk_nstride <<= 1;
	    }
	    chunk_size = chunk_ostride;
	}
	else if (size > 0)
	{
	    /* smaller */
	    while (odim > size * 2)
	    {
		odim >>= 1;
		ents >>= 1;
		chunk_nstride >>= 1;
	    }
	    chunk_size = chunk_nstride;
	}
	else
	{
	    /* empty */
	    ents = 0;
	    chunk_nstride = 0;
	    chunk_size = 0;
	    for (d = 0; d < a->ndim; d++)
	    {
		dims[d] = 0;
		limits[d] = 0;
	    }
	}
	nbox = NewBox (a->values->constant, True, ents, a->values->u.type);
	o = BoxElements (a->values);
	n = BoxElements (nbox);
        chunk_zero = chunk_nstride - chunk_size;
	while (nchunk--)
	{
	    memcpy (n, o, chunk_size * sizeof (Value));
	    o += chunk_ostride;
	    n += chunk_size;
	    memset (n, '\0', chunk_zero * sizeof (Value));
	    n += chunk_zero;
	}
	BoxSetReplace (a->values, nbox, chunk_ostride, chunk_nstride);
	a->values = nbox;
	dims[dim] = odim;
	EXIT ();
    }
    limits[dim] = size;
}

void
ArraySetDimensions (Value av, int *dims)
{
    Array   *a = &av->array;
    int	    i;

    for (i = 0; i < a->ndim; i++)
	ArrayResize (av, i, dims[i]);
}
