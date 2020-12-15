/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

static Value
RefPlus (Value av, Value bv, int expandOk)
{
    ENTER();
    int	    i;
    Ref	    *ref;

    if (ValueIsInt(av))
    {
	i = IntPart (av, "Attempt to add non-integer to reference type");
	if (aborting)
	    RETURN (Void);
	ref = &bv->ref;
    }
    else if (ValueIsInt(bv))
    {
	i = IntPart (bv, "Attempt to add non-integer to reference type");
	if (aborting)
	    RETURN (Void);
	ref = &av->ref;
    }
    else
	RETURN (Void);
    i = i + ref->element;
    if (i < 0 || i >= ref->box->nvalues ||
	(!ref->box->homogeneous && i != ref->element))
    {
	RaiseStandardException (exception_invalid_array_bounds, 2,
				av, bv);
	RETURN (Void);
    }
    RETURN (NewRef (ref->box, i));
}

static Value
RefMinus (Value av, Value bv, int expandOk)
{
    ENTER();
    int	    i;
    int	    element;
    Ref	    *ref, *bref;

    if (ValueIsInt(av))
    {
	i = IntPart (av, "Attempt to subtract non-integer to reference type");
	if (aborting)
	    RETURN (Void);
	ref = &bv->ref;
	element = -ref->element;
    }
    else if (ValueIsInt(bv))
    {
	i = -IntPart (bv, "Attempt to subtract non-integer to reference type");
	if (aborting)
	    RETURN (Void);
	ref = &av->ref;
	element = ref->element;
    }
    else
    {
	ref = &av->ref;
	bref = &bv->ref;
	if (ref->box != bref->box)
	{
	    RaiseStandardException (exception_invalid_binop_values, 2,
				    av, bv);
	    RETURN (Void);
	}
	RETURN (NewInt (ref->element - bref->element));
    }
    i = i + element;
    if (i < 0 || i >= ref->box->nvalues || (!ref->box->homogeneous && i != ref->element))
    {
	RaiseStandardException (exception_invalid_array_bounds, 2,
				av, bv);
	RETURN (Void);
    }
    RETURN (NewRef (ref->box, i));
}

static Value
RefLess (Value av, Value bv, int expandOk)
{
    Ref	*aref = &av->ref, *bref = &bv->ref;

    if (aref->box != bref->box || 
	(!aref->box->homogeneous && aref->element != bref->element))
    {
	RaiseStandardException (exception_invalid_binop_values, 2,
				av, bv);
	return FalseVal;
    }
    if (aref->element < bref->element)
	return TrueVal;
    return FalseVal;
}

static Value
RefEqual (Value av, Value bv, int expandOk)
{
    Ref	*aref = &av->ref, *bref = &bv->ref;

    if (aref->box != bref->box || aref->element != bref->element)
	return FalseVal;
    return TrueVal;
}

static ValueRep *
RefTypeCheck (BinaryOp op, Value av, Value bv, int expandOk)
{
    switch (op) {
    case MinusOp:
	if (ValueIsRef(av) && ValueIsRef(bv))
	    return av->value.type;
    case PlusOp:
	if (ValueIsInt(av))
	    return bv->value.type;
	if (ValueIsInt(bv))
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
RefPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    FileOutput (f, '&');
    return Print (f, RefValueGet (av), format, base, width ? width - 1 : 0, prec, fill);
}
    
static void
RefMark (void *object)
{
    Ref	*ref = object;

    if (ref->box->replace)
	ref->box = BoxRewrite (ref->box, &ref->element);
    MemReference (ref->box);
}

ValueRep RefRep = { 
    { RefMark, 0, "RefRep" },	/* data */
    rep_ref,		/* tag */
    {			/* binary */
	RefPlus,
	RefMinus,
	0,
	0,
	0,
	0,
	RefLess,
	RefEqual,
	0,
	0,
    },
    {			/* unary */
	0,
	0,
	0,
    },
    0,
    0,
    RefPrint,
    RefTypeCheck,
};
    
DataCachePtr	refCache;

Value
NewRefReal (BoxPtr box, int element, Value *re)
{
    ENTER ();
    Value   ret = ALLOCATE (&RefRep.data, sizeof (Ref));
    ret->ref.box = box;
    ret->ref.element = element;
    *re = ret;
    RETURN (ret);
}

void
RefRewrite (Value rv)
{
    Ref	    *ref= &rv->ref;
    BoxPtr  box = ref->box;

    if (box->replace)
	ref->box = BoxRewrite (box, &ref->element);
}

int
RefInit (void)
{
    ENTER ();
    refCache = NewDataCache(REF_CACHE_SIZE);
    EXIT ();
    return 1;
}
