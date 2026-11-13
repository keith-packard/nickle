/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include "nickle.h"

/*
 * Raise invalid_array_bounds for pointer arithmetic leaving the array
 */
static Value
RefOutside(Value av, Value bv)
{
    RaiseStandardException(exception_invalid_array_bounds, 3,
                           NewStrString("pointer arithmetic outside of array"), av, bv);
    return Void;
}

/*
 * The bounds checks compare offsets before adding them to the element
 * index so that offsets near the ends of the ptrdiff_t range cannot
 * overflow
 */
static Value
RefPlus(Value av, Value bv, int expandOk)
{
    ENTER();
    ptrdiff_t i;
    Ref      *ref;
    Value     offset;

    (void)expandOk;
    if (ValueIsRef(av)) {
        ref = &av->ref;
        offset = bv;
    } else if (ValueIsRef(bv)) {
        ref = &bv->ref;
        offset = av;
    } else
        RETURN(Void);
    if (!ValueToPtrdiff(offset, &i) || i < -ref->element || i >= ref->box->nvalues - ref->element
        || (!ref->box->homogeneous && i != 0))
        RETURN(RefOutside(av, bv));
    RETURN(NewRef(ref->box, ref->element + i));
}

static Value
RefMinus(Value av, Value bv, int expandOk)
{
    ENTER();
    ptrdiff_t i;
    Ref      *ref, *bref;

    (void)expandOk;
    if (ValueIsRef(av) && ValueIsRef(bv)) {
        ref = &av->ref;
        bref = &bv->ref;
        if (ref->box != bref->box) {
            RaiseStandardException(exception_invalid_binop_values, 3,
                                   NewStrString("subtracting pointers to different arrays"), av,
                                   bv);
            RETURN(Void);
        }
        RETURN(NewPtrdiffInteger(ref->element - bref->element));
    }
    if (ValueIsRef(av)) {
        ref = &av->ref;
        if (!ValueToPtrdiff(bv, &i) || i > ref->element || i <= ref->element - ref->box->nvalues
            || (!ref->box->homogeneous && i != 0))
            RETURN(RefOutside(av, bv));
        RETURN(NewRef(ref->box, ref->element - i));
    }
    if (ValueIsRef(bv)) {
        ref = &bv->ref;
        if (!ValueToPtrdiff(av, &i) || i < ref->element || i - ref->element >= ref->box->nvalues
            || (!ref->box->homogeneous && i - ref->element != ref->element))
            RETURN(RefOutside(av, bv));
        RETURN(NewRef(ref->box, i - ref->element));
    }
    RETURN(Void);
}

static Value
RefLess(Value av, Value bv, int expandOk)
{
    Ref *aref = &av->ref, *bref = &bv->ref;

    (void)expandOk;
    if (aref->box != bref->box || (!aref->box->homogeneous && aref->element != bref->element)) {
        RaiseStandardException(exception_invalid_binop_values, 3,
                               NewStrString("comparing pointers to different arrays"), av, bv);
        return FalseVal;
    }
    if (aref->element < bref->element)
        return TrueVal;
    return FalseVal;
}

static Value
RefEqual(Value av, Value bv, int expandOk)
{
    Ref *aref = &av->ref, *bref = &bv->ref;

    (void)expandOk;
    if (aref->box != bref->box || aref->element != bref->element)
        return FalseVal;
    return TrueVal;
}

static ValueRep *
RefTypeCheck(BinaryOp op, Value av, Value bv, int expandOk)
{
    (void)expandOk;
    switch (op) {
    case MinusOp:
        if (ValueIsRef(av) && ValueIsRef(bv))
            return av->value.type;
        __NICKLE_FALLTHROUGH;
    case PlusOp:
        if (Integralp(ValueTag(av)))
            return bv->value.type;
        if (Integralp(ValueTag(bv)))
            return av->value.type;
        break;
    case LessOp:
    case EqualOp:
        if (ValueIsRef(av) && ValueIsRef(bv))
            return av->value.type;
        break;
    default:
        break;
    }
    return 0;
}

static Bool
RefPrint(Value f, Value av, uint32_t format, int base, int width, int prec, uint32_t fill)
{
    FileOutput(f, '&');
    return Print(f, RefValueGet(av), format, base, width ? width - 1 : 0, prec, fill);
}

static void
RefMark(void *object)
{
    Ref *ref = object;

    MemReference(ref->box);
}

ValueRep RefRep = {
    { RefMark, 0, "RefRep" }, /* data */
    rep_ref, /* tag */
    {
     /* binary */
        RefPlus, RefMinus,
     0, 0,
     0, 0,
     RefLess, RefEqual,
     0, 0,
     },
    {
     /* unary */
        0, 0,
     0, },
    0,
    0,
    RefPrint,
    RefTypeCheck,
    0,
};

DataCachePtr refCache;

Value
NewRefReal(BoxPtr box, ptrdiff_t element, Value *re)
{
    ENTER();
    Value ret = ALLOCATE(&RefRep.data, sizeof(Ref));
    ret->ref.box = box;
    ret->ref.element = element;
    *re = ret;
    RETURN(ret);
}

int
RefInit(void)
{
    ENTER();
    refCache = NewDataCache(REF_CACHE_SIZE);
    EXIT();
    return 1;
}
