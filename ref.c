/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
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
	    RETURN (Zero);
	ref = &bv->ref;
    }
    else if (ValueIsInt(bv))
    {
	i = IntPart (bv, "Attempt to add non-integer to reference type");
	if (aborting)
	    RETURN (Zero);
	ref = &av->ref;
    }
    else
	RETURN (Zero);
    i = i + ref->element;
    if (i < 0 || i >= ref->box->nvalues ||
	(!ref->box->array && i != ref->element))
    {
	RaiseStandardException (exception_invalid_array_bounds,
				"Element out of range in reference addition",
				2, av, bv);
	RETURN (Zero);
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
	    RETURN (Zero);
	ref = &bv->ref;
	element = -ref->element;
    }
    else if (ValueIsInt(bv))
    {
	i = -IntPart (bv, "Attempt to subtract non-integer to reference type");
	if (aborting)
	    RETURN (Zero);
	ref = &av->ref;
	element = ref->element;
    }
    else
    {
	ref = &av->ref;
	bref = &bv->ref;
	if (ref->box != bref->box)
	{
	    RaiseError ("Attempt to subtract references to different objects %v - %v", av, bv);
	    RETURN (Zero);
	}
	RETURN (NewInt (ref->element - bref->element));
    }
    i = i + element;
    if (i < 0 || i >= ref->box->nvalues || (!ref->box->array && i != ref->element))
    {
	RaiseStandardException (exception_invalid_array_bounds,
				"Element out of range in reference subtraction",
				2, av, bv);
	RETURN (Zero);
    }
    RETURN (NewRef (ref->box, i));
}

static Value
RefLess (Value av, Value bv, int expandOk)
{
    Ref	*aref = &av->ref, *bref = &bv->ref;

    if (aref->box != bref->box || 
	(!aref->box->array && aref->element != bref->element))
    {
	RaiseError ("Attempt to order references to different objects %v < %v",
		    av, bv);
	return Zero;
    }
    if (aref->element < bref->element)
	return One;
    return Zero;
}

static Value
RefEqual (Value av, Value bv, int expandOk)
{
    Ref	*aref = &av->ref, *bref = &bv->ref;

    if (aref->box != bref->box || aref->element != bref->element)
	return Zero;
    return One;
}

static ValueType *
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
RefPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FileOutput (f, '&');
    return Print (f, RefValueGet (av), format, base, width ? width - 1 : 0, prec, fill);
}

    
static void
RefMark (void *object)
{
    Ref	*ref = object;

    MemReference (ref->box);
}

ValueType RefType = { 
    { RefMark, 0 },	/* data */
    type_ref,		/* tag */
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
    
Value
NewRef (BoxPtr box, int element)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&RefType.data, sizeof (Ref));
    ret->ref.box = box;
    ret->ref.element = element;
    RETURN (ret);
}
