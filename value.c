/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * operators accepting values
 */

#include	"nickle.h"

Value	Void;
Value	TrueVal, FalseVal;

volatile Bool	aborting;
volatile Bool	signaling;

#ifndef Numericp
Bool
Numericp (Rep t)
{
    switch (t) {
    case rep_int:
    case rep_integer:
    case rep_rational:
    case rep_float:
	return True;
    default:;
    }
    return False;
}
#endif

#ifndef Integralp
Bool
Integralp (Rep t)
{
    switch (t) {
    case rep_int:
    case rep_integer:
	return True;
    default:;
    }
    return False;
}
#endif

Bool
Zerop (Value av)
{
    switch (ValueTag(av)) {
    case rep_int:
	return av->ints.value == 0;
    case rep_integer:
	return av->integer.mag->length == 0;
    case rep_rational:
	return av->rational.num->length == 0;
    case rep_float:
	return av->floats.mant->mag->length == 0;
    default:;
    }
    return False;
}

Bool
Negativep (Value av)
{
    switch (ValueTag(av)) {
    case rep_int:
	return av->ints.value < 0;
    case rep_integer:
	return av->integer.sign == Negative;
    case rep_rational:
	return av->rational.sign == Negative;
    case rep_float:
	return av->floats.mant->sign == Negative;
    default:;
    }
    return False;
}

Bool
Evenp (Value av)
{
    switch (ValueTag(av)) {
    case rep_int:
	return (av->ints.value & 1) == 0;
    case rep_integer:
	return NaturalEven (av->integer.mag);
    default:;
    }
    return False;
}

int
IntPart (Value av, char *error)
{
    if (!ValueIsInt(av))
    {
	RaiseStandardException (exception_invalid_argument, error, 
				2, NewInt (0), av);
	return 0;
    }
    return av->ints.value;
}

Value
BinaryOperate (Value av, Value bv, BinaryOp operator)
{
    ENTER ();
    Value	ret;
    ValueRep	*type = 0;

    if (av->value.type->typecheck)
	type = (*av->value.type->typecheck) (operator, av, bv, 1);
    else if (bv->value.type->typecheck)
	type = (*bv->value.type->typecheck) (operator, av, bv, 1);
    else if (av->value.type == bv->value.type)
	type = av->value.type;
    else if (Numericp (ValueTag(av)) && Numericp (ValueTag(bv)))
    {
	if (ValueTag(av) < ValueTag(bv))
	    av = (*bv->value.type->promote) (av, bv);
	else
	    bv = (*av->value.type->promote) (bv, av);
	type = av->value.type;
    }
    else if (ValueIsUnion(av))
	type = av->value.type;
    else if (ValueIsUnion(bv))
	type = bv->value.type;
    if (!type || !type->binary[operator])
    {
	if (operator == EqualOp)
	    RETURN (FalseVal);
	RaiseStandardException (exception_invalid_binop_values,
				"invalid operands",
				2,
				av, bv);
	RETURN (Void);
    }
    if (aborting)
	RETURN (Void);
    ret = (*type->binary[operator]) (av, bv, 1);
    if (ret->value.type->reduce)
	ret = (*ret->value.type->reduce) (ret);
    RETURN (ret);
}

Value
UnaryOperate (Value v, UnaryOp operator)
{
    ENTER ();
    Value   ret;
    
    if (!v->value.type->unary[operator])
    {
	RaiseStandardException (exception_invalid_unop_value,
				"invalid operand",
				1, v);
	RETURN (Void);
    }
    if (aborting)
	RETURN (Void);
    ret = (*v->value.type->unary[operator])(v, 1);
    if (ret->value.type->reduce)
	ret = (*ret->value.type->reduce) (ret);
    RETURN (ret);
}

Value
Reduce (Value v)
{
    ENTER ();
    if (v->value.type->reduce)
	v = (*v->value.type->reduce) (v);
    RETURN (v);
}

Value
Plus (Value av, Value bv)
{
    return BinaryOperate (av, bv, PlusOp);
}

Value
Minus (Value av, Value bv)
{
    return BinaryOperate (av, bv, MinusOp);
}

Value
Times (Value av, Value bv)
{
    return BinaryOperate (av, bv, TimesOp);
}

Value
Divide (Value av, Value bv)
{
    return BinaryOperate (av, bv, DivideOp);
}

Value
Div (Value av, Value bv)
{
    return BinaryOperate (av, bv, DivOp);
}

Value
Mod (Value av, Value bv)
{
    return BinaryOperate (av, bv, ModOp);
}

Value
NumericDiv (Value av, Value bv, int expandOk)
{
    ENTER ();
    RETURN (Floor (Divide (av, bv)));
}

Value
Less (Value av, Value bv)
{
    return BinaryOperate (av, bv, LessOp);
}

Value
Equal (Value av, Value bv)
{
    return BinaryOperate (av, bv, EqualOp);
}

Value
Land (Value av, Value bv)
{
    return BinaryOperate (av, bv, LandOp);
}

Value
Lor (Value av, Value bv)
{
    return BinaryOperate (av, bv, LorOp);
}

Value
Negate (Value av)
{
    return UnaryOperate (av, NegateOp);
}

Value
Floor (Value av)
{
    return UnaryOperate (av, FloorOp);
}

Value
Ceil (Value av)
{
    return UnaryOperate (av, CeilOp);
}

/*
 * non primitive functions
 */

Value
Lnot (Value av)
{
    ENTER ();
    RETURN (Minus (Negate (av), One));
}

Value
Lxor (Value av, Value bv)
{
    ENTER ();
    RETURN (Land (Lnot (Land (av, bv)),
		  Lor (av, bv))); 
}

Value
Not (Value av)
{
    ENTER ();

    if (True (av))
	av = FalseVal;
    else
	av = TrueVal;
    RETURN (av);
}

Value
Greater (Value av, Value bv)
{
    return Less (bv, av);
}

Value
LessEqual (Value av, Value bv)
{
    return Not (Less (bv, av));
}

Value
GreaterEqual (Value av, Value bv)
{
    return Not (Less (av, bv));
}

Value
NotEqual (Value av, Value bv)
{
    return Not (Equal (av, bv));
}

Value
Factorial (Value av)
{
    ENTER ();
    Value   tv;
    Value   i;    
    StackPointer    iref, tvref;

    if (!Integralp (ValueTag(av)) || Negativep (av))
    {
	RaiseStandardException (exception_invalid_unop_value,
				"invalid operand",
				1,
				av);
	RETURN (Void);
    }
    /*
     * A bit of reference magic here to avoid churning
     * through megabytes.  Build a couple of spots
     * on the reference stack for the two intermediate
     * values and then reuse them after each iteration
     */
    tv = One;
    i = One;
    REFERENCE (tv);
    tvref = STACK_TOP(MemStack);
    REFERENCE (i);
    iref = STACK_TOP(MemStack);
    for (;;)
    {
	ENTER ();
	if (aborting || False (Less (i, av)))
	{
	    EXIT ();
	    break;
	}
	i = Plus (i, One);
	tv = Times (i, tv);
	EXIT ();
	*iref = i;
	*tvref = tv;
    }
    RETURN (tv);
}

Value
Truncate (Value av)
{
    ENTER ();
    if (Negativep (av))
	av = Ceil (av);
    else
	av = Floor (av);
    RETURN (av);
}

Value
Round (Value av)
{
    ENTER ();
    RETURN (Floor (Plus (av, NewRational (Positive, one_natural, two_natural))));
}

Value
Pow (Value av, Value bv)
{
    ENTER ();
    Value	result;

    if (!Numericp (ValueTag(av)) || !Numericp (ValueTag(bv)))
    {
	RaiseStandardException (exception_invalid_binop_values,
				"invalid operands",
				2,
				av, bv);
	RETURN (Void);
    }
    switch (ValueTag(bv)) {
    case rep_int:
	{
	    Value	p;
	    int		i;
	    int		flip = 0;

	    i = bv->ints.value;
	    if (i < 0)
	    {
		i = -i;
		flip = 1;
	    }
	    p = av;
	    result = One;
	    while (i) {
		if (aborting)
		    RETURN (Void);
		if (i & 1)
		    result = Times (result, p);
		i >>= 1;
		if (i)
		    p = Times (p, p);
	    }
	    if (flip)
		result = Divide (One, result);
	}
	break;
    case rep_integer:
	{
	    Value   p;
	    Natural *i;
	    Natural *two;
	    Natural *rem;
	    int	    flip = 0;

	    i = bv->integer.mag;
	    if (bv->integer.sign == Negative)
		flip = 1;
	    two = NewNatural (2);
	    p = av;
	    result = One;
	    while (!NaturalZero (i)) {
		if (aborting)
		    RETURN (Void);
		if (!NaturalEven (i))
		    result = Times (result, p);
		i = NaturalDivide (i, two, &rem);
		if (!NaturalZero (i))
		    p = Times (p, p);
	    }
	    if (flip)
		result = Divide (One, result);
	}
	break;
    default:
	RaiseError ("pow: non-integer power %v", bv);
	result = Void;
	break;
    }
    RETURN (result);
}

Value
ShiftL (Value av, Value bv)
{
    ENTER ();
    if (!Integralp (ValueTag(av)) || !Integralp (ValueTag(bv)))
    {
	RaiseError ("non-integer %v << %v\n", av, bv);
	RETURN (Void);
    }
    if (Negativep (bv))
	RETURN (ShiftR(av, Negate (bv)));
    if (Zerop (bv))
	RETURN(av);
    if (ValueIsInt(bv))
    {
	Sign	sign = Positive;
	if (Negativep (av))
	    sign = Negative;
	av = Reduce (NewInteger (sign,
				 NaturalLsl (IntegerRep.promote (av,0)->integer.mag,
					     bv->ints.value)));
    }
    else
    {
	av = Times (av, Pow (NewInt(2), bv));
    }
    RETURN (av);
}

Value
ShiftR (Value av, Value bv)
{
    ENTER ();
    if (!Integralp (ValueTag(av)) || !Integralp (ValueTag(bv)))
    {
	RaiseError ("non-integer %v >> %v\n", av, bv);
	RETURN (Void);
    }
    if (Negativep (bv))
	RETURN (ShiftL(av, Negate (bv)));
    if (Zerop (bv))
	RETURN(av);
    if (ValueIsInt(bv))
    {
	Sign	sign = Positive;
	if (Negativep (av))
	{
	    av = Minus (av, ShiftL (One, Minus (bv, One)));
	    sign = Negative;
	}
	av = Reduce (NewInteger (sign,
				 NaturalRsl (IntegerRep.promote (av,0)->integer.mag,
					     bv->ints.value)));
    }
    else
    {
	av = Div (av, Pow (NewInt(2), bv));
    }
    RETURN (av);
}

Value
Gcd (Value av, Value bv)
{
    ENTER ();
    
    if (!Integralp (ValueTag(av)) || !Integralp (ValueTag(bv)))
    {
	RaiseStandardException (exception_invalid_binop_values,
				"invalid gcd argument values",
				2,
				av, bv);
	RETURN (Void);
    }
    RETURN (Reduce (NewInteger (Positive, 
				NaturalGcd (IntegerRep.promote (av, 0)->integer.mag,
					    IntegerRep.promote (bv, 0)->integer.mag))));
}

#ifdef GCD_DEBUG
Value
Bdivmod (Value av, Value bv)
{
    ENTER ();
    
    if (!Integralp (ValueTag(av)) || !Integralp (ValueTag(bv)))
    {
	RaiseStandardException (exception_invalid_binop_values,
				"invalid gcd argument values",
				2,
				av, bv);
	RETURN (Void);
    }
    RETURN (Reduce (NewInteger (Positive,
				NaturalBdivmod (IntegerRep.promote (av, 0)->integer.mag,
						IntegerRep.promote (bv, 0)->integer.mag))));
}

Value
KaryReduction (Value av, Value bv)
{
    ENTER ();
    
    if (!Integralp (ValueTag(av)) || !Integralp (ValueTag(bv)))
    {
	RaiseStandardException (exception_invalid_binop_values,
				"invalid kary_reduction argument values",
				2,
				av, bv);
	RETURN (Void);
    }
    RETURN (Reduce (NewInteger (Positive,
				NaturalKaryReduction (IntegerRep.promote (av, 0)->integer.mag,
						      IntegerRep.promote (bv, 0)->integer.mag))));
}
#endif

StackObject *ValuePrintStack;
int	    ValuePrintLevel;

Bool
Print (Value f, Value v, char format, int base, int width, int prec, unsigned char fill)
{
    int	    i;
    Bool    ret;
    
    if (!v)
    {
	FilePuts (f, "<uninit>");
	return True;
    }
    if (!v->value.type->print)
	return True;
    for (i = 0; i < ValuePrintLevel; i++)
    {
	if (STACK_ELT(ValuePrintStack, i) == v)
	{
	    FilePuts (f, "<recursive>");
	    return True;
	}
    }
    STACK_PUSH (ValuePrintStack, v);
    ++ValuePrintLevel;
    ret = (*v->value.type->print) (f, v, format, base, width, prec, fill);
    STACK_POP (ValuePrintStack);
    --ValuePrintLevel;
    return ret;
}

/*
 * Make a deep copy of 'v'
 */
Value
CopyMutable (Value v)
{
    ENTER ();
    Value   nv;
    int	    i;
    BoxPtr  box, nbox;
    int	    n;
    
    switch (ValueTag(v)) {
    case rep_array:
	if (v->array.values->constant)
	    RETURN (v);
	nv = NewArray (False, v->array.type, v->array.ndim, v->array.dim);
	box = v->array.values;
	nbox = nv->array.values;
	n = v->array.ents;
	break;
    case rep_struct:
	if (v->structs.values->constant)
	    RETURN (v);
	nv = NewStruct (v->structs.type, False);
	box = v->structs.values;
	nbox = nv->structs.values;
	n = v->structs.type->nelements;
	break;
    case rep_union:
	if (v->unions.value->constant)
	    RETURN (v);
	nv = NewUnion (v->unions.type, False);
	nv->unions.tag = v->unions.tag;
	box = v->unions.value;
	nbox = nv->unions.value;
	n = 1;
	break;
    default:
	RETURN (v);
    }
    for (i = 0; i < n; i++)
	BoxValueSet (nbox, i, Copy (BoxValueGet (box, i)));
    RETURN (nv);
}

#ifndef HAVE_C_INLINE
Value
Copy (Value v)
{
    if (Mutablep (ValueTag(v)))
	return CopyMutable (v);
    return v;
}
#endif

Value
ValueEqual (Value a, Value b, int expandOk)
{
    return a == b ? TrueVal : FalseVal;
}

Value
Dereference (Value v)
{
    ENTER ();
    if (!ValueIsRef(v))
    {
	RaiseStandardException (exception_invalid_unop_value,
				"Not a reference",
				1, v);
	RETURN (Void);
    }
    RETURN (RefValue (v));
}

static Value
UnitEqual (Value av, Value bv, int expandOk)
{
    return TrueVal;
}

static Bool
UnitPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FilePuts (f, "<>");
    return True;
}

ValueRep UnitRep = {
    { 0, 0 },	    /* data */
    rep_void,	    /* tag */
    { 
	0,	    /* Plus */
	0,	    /* Minus */
	0,	    /* Times */
	0,	    /* Divide */
	0,	    /* Div */
	0,	    /* Mod */
	0,	    /* Less */
	UnitEqual,  /* Equal */
	0,	    /* Land */
	0,	    /* Lor */
    },	    /* binary */
    { 0 },	    /* unary */
    0, 0,
    UnitPrint,	    /* print */
};

static Value
NewVoid (void)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&UnitRep.data, sizeof (BaseValue));
    RETURN (ret);
}

static Value
BoolEqual (Value av, Value bv, int expandOk)
{
    return (av == TrueVal) == (bv == TrueVal) ? TrueVal : FalseVal;
}

static Bool
BoolPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FilePuts (f, av == TrueVal ? "true" : "false");
    return True;
}

ValueRep BoolRep = {
    { 0, 0 },	    /* data */
    rep_bool,	    /* tag */
    { 
	0,	    /* Plus */
	0,	    /* Minus */
	0,	    /* Times */
	0,	    /* Divide */
	0,	    /* Div */
	0,	    /* Mod */
	0,	    /* Less */
	BoolEqual,  /* Equal */
	0,	    /* Land */
	0,	    /* Lor */
    },	    /* binary */
    { 0 },	    /* unary */
    0, 0,
    BoolPrint,	    /* print */
};

static Value
NewBool (void)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&BoolRep.data, sizeof (BaseValue));
    RETURN (ret);
}

/*
 * This is a bit odd, but it's just a cache so
 * erase it at GC time
 */
static void
ValueCacheMark (void *object)
{
    ValueCache	*vc = object;

    memset (ValueCacheValues (vc), '\0', sizeof (Value) * vc->size);
}

static DataType ValueCacheType = { ValueCacheMark, 0 };

ValueCachePtr 
NewValueCache (int size)
{
    ENTER ();
    ValueCachePtr   vc;
    vc = (ValueCachePtr) MemAllocate (&ValueCacheType, 
				      sizeof (ValueCache) +
				      size * sizeof (Value));
    vc->size = size;
    MemAddRoot (vc);
    memset (ValueCacheValues(vc), '\0', size * sizeof (Value));
    RETURN (vc);
}

int
ValueInit (void)
{
    if (!AtomInit ())
	return 0;
    if (!ArrayInit ())
	return 0;
    if (!FileInit ())
	return 0;
    if (!IntInit ())
	return 0;
    if (!NaturalInit ())
	return 0;
    if (!RationalInit ())
	return 0;
    if (!FpartInit ())
	return 0;
    if (!RefInit ())
	return 0;
    if (!StringInit ())
	return 0;
    if (!StructInit ())
	return 0;
    ValuePrintStack = StackCreate ();
    MemAddRoot (ValuePrintStack);
    Void = NewVoid ();
    MemAddRoot (Void);
    TrueVal = NewBool ();
    MemAddRoot (TrueVal);
    FalseVal = NewBool ();
    MemAddRoot (FalseVal);
    ValuePrintLevel = 0;
    return 1;
}
