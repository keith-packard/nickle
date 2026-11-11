/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include "nickle.h"
#include "gram.h"

int
ArrayInit(void)
{
    return 1;
}

static ptrdiff_t
ArrayNextI(Array *a, ptrdiff_t i)
{
    ptrdiff_t step = 1;
    ptrdiff_t sub_size = 1;
    int32_t   d;
    ptrdiff_t dim, lim;
    ptrdiff_t j = i + 1;

    for (d = 0; d < a->ndim - 1; d++) {
        dim = ArrayDims(a)[d];
        lim = ArrayLimits(a)[d];
        if (dim != lim && j % dim == lim)
            step += (dim - lim) * sub_size;
        sub_size *= ArrayDims(a)[d];
        j /= dim;
    }
    return i + step;
}

static ptrdiff_t
ArrayLimit(Value av)
{
    Array     *a = &av->array;
    ptrdiff_t *limits = ArrayLimits(a);
    ptrdiff_t *dims = ArrayDims(a);
    int32_t    d;
    ptrdiff_t  limit;

    limit = limits[a->ndim - 1];
    for (d = 0; d < a->ndim - 1; d++)
        limit *= dims[d];
    return limit;
}

static Value
ArrayEqual(Value av, Value bv, int expandOk)
{
    Array    *a = &av->array, *b = &bv->array;
    ptrdiff_t ai, bi;
    ptrdiff_t alimit = ArrayLimit(av), blimit = ArrayLimit(bv);
    int32_t   d;

    (void)expandOk;
    if (a->ndim != b->ndim)
        return FalseVal;
    for (d = 0; d < a->ndim; d++)
        if (ArrayLimits(a)[d] != ArrayLimits(b)[d])
            return FalseVal;
    ai = 0;
    bi = 0;
    while (ai < alimit && bi < blimit) {
        if (False(Equal(ArrayValue(a, ai), ArrayValue(b, bi))))
            return FalseVal;
        ai = ArrayNextI(a, ai);
        bi = ArrayNextI(b, bi);
    }
    return TrueVal;
}

static Bool
ArrayPrint(Value f, Value av, uint32_t format, int base, int width, int prec, uint32_t fill)
{
    ENTER();
    Array     *a = &av->array;
    ptrdiff_t *limits = ArrayLimits(a);
    ptrdiff_t *dims = ArrayDims(a);
    ptrdiff_t  i, j;
    int32_t    d, k;
    int32_t    ndone;
    ptrdiff_t  limit = ArrayLimit(av);
    Bool       ret = True;
    Bool       pretty = format == 'v' || format == 'g' || format == 'G';
    uint32_t   down_format = format == 'g' ? 'G' : format;

    if (pretty) {
        FilePuts(f, "(");
        if (!TypePoly(ArrayType(a))) {
            FilePutBaseType(f, ArrayType(a), False);
            FilePuts(f, " ");
        }
        FilePuts(f, "[");
        for (d = a->ndim - 1; d >= 0; d--) {
            if (a->resizable)
                FilePuts(f, "...");
            else
                FilePutPtrdiff(f, limits[d]);
            if (d)
                FilePuts(f, ", ");
        }
        FilePuts(f, "]");
        if (!TypePoly(ArrayType(a))) {
            FilePutSubscriptType(f, ArrayType(a), False);
        }
        FilePuts(f, ") ");
        if (format != 'G') {
            for (d = 0; d < a->ndim; d++)
                FileOutput(f, '{');
        }
    }
    if (format != 'G') {
        i = 0;
        while (i < limit) {
            if (!Print(f, ArrayValueGet(a, i), down_format, base, width, prec, fill)) {
                ret = False;
                break;
            }
            i = ArrayNextI(a, i);
            if (i < limit) {
                ndone = 0;
                if (pretty) {
                    j = i;
                    k = 0;
                    while (k < a->ndim - 1 && j % dims[k] == 0) {
                        ndone++;
                        j = j / dims[k];
                        k++;
                    }
                    for (k = 0; k < ndone; k++)
                        FileOutput(f, '}');
                    FileOutput(f, ',');
                }
                FileOutput(f, ' ');
                if (pretty)
                    for (k = 0; k < ndone; k++)
                        FileOutput(f, '{');
            }
        }
        if (pretty)
            for (d = 0; d < a->ndim; d++)
                FileOutput(f, '}');
    }
    EXIT();
    return ret;
}

#define hrot(i) (((i) << 1) | ((i) >> (sizeof(HashValue) * 8 - 1)))

static HashValue
ArrayHash(Value av)
{
    Array    *a = &av->array;
    ptrdiff_t i;
    HashValue h = 0;
    ptrdiff_t limit = ArrayLimit(av);

    for (i = 0; i < limit; i = ArrayNextI(a, i))
        h = hrot(h) ^ ValueInt(ValueHash(ArrayValueGet(a, i)));
    return h;
}

static void
ArrayMark(void *object)
{
    Array *array = object;

    if (array->resizable)
        MemReference(array->u.resize);
    else
        MemReference(array->u.fix);
}

ValueRep ArrayRep = {
    { ArrayMark, 0, "ArrayRep" },
    rep_array,
    {
     0, 0,
     0, 0,
     0, 0,
     0, ArrayEqual,
     0, 0,
     },
    {
     0, },
    0,
    0,
    ArrayPrint,
    0,
    ArrayHash,
};

static void
BoxVectorMark(void *object)
{
    BoxVectorPtr bv = object;
    ptrdiff_t    i;
    BoxPtr      *boxes = BoxVectorBoxes(bv);

    MemReference(bv->type);
    for (i = 0; i < bv->nvalues; i++)
        MemReference(boxes[i]);
}

DataType BoxVectorType = { BoxVectorMark, 0, "BoxVectorType" };

static BoxVectorPtr
NewBoxVector(ptrdiff_t nvalues, TypePtr type)
{
    ENTER();
    BoxVectorPtr bv
        = ALLOCATE(&BoxVectorType, sizeof(BoxVector) + (size_t)nvalues * sizeof(BoxPtr));
    ptrdiff_t i;
    BoxPtr   *boxes = BoxVectorBoxes(bv);

    bv->nvalues = nvalues;
    bv->type = type;
    for (i = 0; i < nvalues; i++)
        boxes[i] = 0;
    RETURN(bv);
}

static void
FillBoxVector(BoxVectorPtr bv, BoxPtr *boxes, ptrdiff_t n)
{
    ENTER();
    while (n--)
        *boxes++ = NewBox(False, False, 1, bv->type);
    EXIT();
}

/*
 * Check that an array with these dimensions can be represented,
 * raising out_of_memory when it can't
 */
Bool
ArrayDimsOk(int32_t ndim, ptrdiff_t *dims)
{
    ptrdiff_t ents = 1;
    int32_t   d;

    for (d = 0; d < ndim; d++) {
        if (dims[d] && ents > ARRAY_MAX_VALUES / dims[d]) {
            RaiseStandardException(exception_out_of_memory, 1, NewStrString("array too large"));
            return False;
        }
        ents *= dims[d];
    }
    return True;
}

Value
NewArray(Bool constant, Bool resizable, TypePtr type, int32_t ndim, ptrdiff_t *dims)
{
    ENTER();
    Value     ret;
    ptrdiff_t ents;
    int32_t   dim;

    if (ndim) {
        ents = 1;
        for (dim = 0; dim < ndim; dim++)
            ents *= dims[dim];
    } else
        ents = 0;
    ret = ALLOCATE(&ArrayRep.data, sizeof(Array) + (size_t)ndim * 2 * sizeof(ptrdiff_t));
    ret->array.ndim = ndim;
    for (dim = 0; dim < ndim; dim++)
        ArrayLimits(&ret->array)[dim] = ArrayDims(&ret->array)[dim] = dims[dim];
    ret->array.resizable = resizable;
    if (resizable) {
        ret->array.u.resize = 0;
        ret->array.u.resize = NewBoxVector(ents, type);
        FillBoxVector(ret->array.u.resize, BoxVectorBoxes(ret->array.u.resize), ents);
    } else {
        ret->array.u.fix = 0;
        ret->array.u.fix = NewBox(constant, True, ents, type);
    }
    RETURN(ret);
}

void
ArrayResize(Value av, int32_t dim, ptrdiff_t size)
{
    ENTER();
    Array     *a = &av->array;
    ptrdiff_t *dims = ArrayDims(a);
    ptrdiff_t *limits = ArrayLimits(a);
    ptrdiff_t  odim = dims[dim];
    ptrdiff_t  ents;
    int32_t    d;
    ptrdiff_t  stride; /* size of each chunk */
    ptrdiff_t  nchunk; /* number of chunks */
    ptrdiff_t  c;
    ptrdiff_t  unit;
    ptrdiff_t  good;
    BoxPtr    *b;

    assert(av->array.resizable);

    if (size == limits[dim])
        return;

    ents = a->u.resize->nvalues;

    stride = 1;
    for (d = 0; d <= dim; d++)
        stride *= dims[d];
    if (stride)
        nchunk = ents / stride;
    else
        nchunk = 1;

    /*
     * Resize if necessary.
     */
    if (dims[dim] < size || dims[dim] > size * 2) {
        ptrdiff_t    ostride = stride;
        ptrdiff_t    nstride = stride;
        BoxVectorPtr nboxes;
        BoxPtr      *o, *n;

        if (odim < size) {
            /* make sure the grown array can be represented */
            ptrdiff_t others = odim ? ents / odim : 1;

            if (others && size > ARRAY_MAX_VALUES / 2 / others) {
                RaiseStandardException(exception_out_of_memory, 1, NewStrString("array too large"));
                EXIT();
                return;
            }
            /* was empty */
            if (odim == 0) {
                odim = 1;
                ents = 1;
                nstride = 1;
                for (d = 0; d < a->ndim; d++)
                    dims[d] = 1;
            }
            /* bigger */
            while (odim < size) {
                odim <<= 1;
                ents <<= 1;
                nstride <<= 1;
            }
            good = ostride;
        } else if (size > 0) {
            /* smaller */
            while (odim > size * 2) {
                odim >>= 1;
                ents >>= 1;
                nstride >>= 1;
            }
            good = nstride;
        } else {
            /* empty */
            ents = 0;
            nstride = 0;
            odim = 0;
            size = 0;
            nchunk = 0;
            for (d = 0; d < a->ndim; d++) {
                dims[d] = 0;
                limits[d] = 0;
            }
            good = 0;
        }

        nboxes = NewBoxVector(ents, a->u.resize->type);
        o = BoxVectorBoxes(a->u.resize);
        n = BoxVectorBoxes(nboxes);
        for (c = 0; c < nchunk; c++) {
            memcpy(n, o, (size_t)good * sizeof(BoxPtr));
            if (nstride > good)
                FillBoxVector(nboxes, n + good, nstride - good);
            o += ostride;
            n += nstride;
        }
        a->u.resize = nboxes;
        dims[dim] = odim;
        limits[dim] = size;
        stride = nstride;
    }
    /*
     * When shrinking the array, replace the
     * now discarded entries with new boxes.
     * This leaves growing trivial; all unused
     * elements of the array have clean boxes
     */
    if (limits[dim] > size) {
        b = BoxVectorBoxes(a->u.resize);

        unit = stride / dims[dim];
        good = size * unit;

        for (c = 0; c < nchunk; c++) {
            FillBoxVector(a->u.resize, b + good, stride - good);
            b += stride;
        }
    }
    limits[dim] = size;
    EXIT();
}

void
ArraySetDimensions(Value av, ptrdiff_t *dims)
{
    Array  *a = &av->array;
    int32_t i;

    for (i = 0; i < a->ndim && !aborting; i++)
        ArrayResize(av, i, dims[i]);
}

Type *
BuildArrayType(Type *subtype, int32_t ndim, ...)
{
    ENTER();
    Expr     *dims = 0;
    ptrdiff_t i;
    int       dim;
    va_list   ap;
    Type     *type;
    Value     dimArray;
    ptrdiff_t ndims = ndim;

    dimArray = NewArray(True, False, typePrim[rep_integer], 1, &ndims);
    va_start(ap, ndim);
    for (i = 0; i < ndim; i++) {
        dim = va_arg(ap, int);
        ArrayValueSet(&dimArray->array, i, NewInt(dim));
        dims = NewExprTree(COMMA, NewExprConst(TEN_FLOAT, NewInt(dim)), dims);
    }
    va_end(ap);
    type = NewTypeArray(subtype, dims, False);
    /*
     * Create an array to hold the dimension information and
     * fill it in
     */
    type->array.storage = DimStorageGlobal;
    type->array.u.global = NewBox(True, False, 1, typeArrayInt);
    BoxValueSet(type->array.u.global, 0, dimArray);
    RETURN(type);
}
