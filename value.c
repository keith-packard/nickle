/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
	return ValueInt(av) == 0;
    case rep_integer:
	return IntegerMag(av)->length == 0;
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
	return ValueInt(av) < 0;
    case rep_integer:
	return IntegerSign(av) == Negative;
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
	return (ValueInt(av) & 1) == 0;
    case rep_integer:
	return NaturalEven (IntegerMag(av));
    default:;
    }
    return False;
}

int
IntPart (Value av, char *error)
{
    if (!ValueIsInt(av))
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString (error), 
				NewInt (0), av);
	return 0;
    }
    return ValueInt(av);
}

int
BoolPart (Value av, char *error)
{
    if (!ValueIsBool(av))
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString (error), 
				NewInt (0), av);
	return 0;
    }
    return av == TrueVal;
}

signed_digit
SignedDigitPart(Value av, char *error)
{
    if (ValueIsInt(av))
	return ValueInt(av);
    if (ValueIsInteger(av) && IntegerFitsSignedDigit(&av->integer))
	return IntegerToSignedDigit(&av->integer);

    RaiseStandardException (exception_invalid_argument, 3,
			    NewStrString (error), 
			    NewInt (0), av);
    return 0;
}

Value
BinaryOperate (Value av, Value bv, BinaryOp operator)
{
    if (ValueIsInt(av) && ValueIsInt(bv))
    {
	int	a, b, r;
	signed_digit rd;
	switch (operator) {
	case PlusOp:
	    r = ValueInt(av) + ValueInt(bv);
    
	    if (NICKLE_INT_CARRIED(r))
		return Plus (NewIntInteger (ValueInt(av)), NewIntInteger(ValueInt(bv)));
	    return NewInt(r);
	case MinusOp:
	    r = ValueInt(av) - ValueInt(bv);
    
	    if (NICKLE_INT_CARRIED(r))
		return Minus (NewIntInteger (ValueInt(av)), NewIntInteger(ValueInt(bv)));
	    return NewInt(r);    
	case TimesOp:
	    a = ValueInt(av), b = ValueInt(bv);
	    rd = (signed_digit) a * (signed_digit) b;
    
	    if (rd > (signed_digit) MAX_NICKLE_INT || rd < (signed_digit) MIN_NICKLE_INT)
		return NewSignedDigitInteger (rd);
	    return NewInt ((int) rd);
	case DivideOp:
	    a = ValueInt(av), b = ValueInt(bv);
    
	    if (b == 0)
	    {
		RaiseStandardException (exception_divide_by_zero, 2,
					av, bv);
		return Void;
	    }
	    if (a % b != 0)
		return Divide (NewIntRational (a), NewIntRational (b));
	    return NewInt (a/b);
	case DivOp:
	    a = ValueInt(av), b = ValueInt(bv);
    
	    if (b == 0)
	    {
		RaiseStandardException (exception_divide_by_zero, 2,
					av, bv);
		return Void;
	    }
	    switch (catagorize_signs (IntSign(a), IntSign(b))) {
	    case BothPositive:
		r = a / b;
		break;
	    case FirstPositive:
		r = - (a / -b);
		break;
	    case SecondPositive:
		r = -(-a / b);
		if (-a % b)
		    --r;
		break;
	    case BothNegative:
	    default:
		r = -a / -b;
		if (-a % -b)
		    r++;
		break;
	    }
	    return NewInt (r);
	case ModOp:
	    a = ValueInt(av), b = ValueInt(bv);
    
	    if (b == 0)
	    {
		RaiseStandardException (exception_divide_by_zero, 2,
					av, bv);
		return Void;
	    }
	    switch (catagorize_signs (IntSign(a), IntSign(b))) {
	    case BothPositive:
		r = a % b;
		break;
	    case FirstPositive:
		r = a % -b;
		break;
	    case SecondPositive:
		r = -a % b;
		if (r)
		    r = b - r;
		break;
	    case BothNegative:
	    default:
		r = -a % -b;
		if (r)
		    r = -b - r;
		break;
	    }
	    return NewInt (r);
	case EqualOp:
	    return av == bv ? TrueVal : FalseVal;
	case LessOp:
	    return ValueInt(av) < ValueInt(bv) ? TrueVal : FalseVal;
	case LandOp:
	    return NewInt (ValueInt(av) & ValueInt(bv));
	case LorOp:
	    return NewInt (ValueInt(av) | ValueInt(bv));
	case NumBinaryOp:
	    ;
	}
	return Void;
    }
    else
    {
	ENTER ();
	Value	ret;
	ValueRep	*arep = ValueRep(av), *brep = ValueRep(bv);
	ValueRep	*rep = 0;
    
	if (arep->typecheck)
	    rep = (*arep->typecheck) (operator, av, bv, 1);
	else if (brep->typecheck)
	    rep = (*brep->typecheck) (operator, av, bv, 1);
	else if (arep == brep)
	    rep = arep;
	else if (Numericp (ValueTag(av)) && Numericp (ValueTag(bv)))
	{
	    if (ValueTag(av) < ValueTag(bv))
		av = (*brep->promote) (av, bv);
	    else
		bv = (*arep->promote) (bv, av);
	    rep = ValueRep(av);
	}
	else if (ValueIsUnion(av))
	    rep = arep;
	else if (ValueIsUnion(bv))
	    rep = brep;
	if (!rep || !rep->binary[operator])
	{
	    if (operator == EqualOp)
		RETURN (FalseVal);
	    RaiseStandardException (exception_invalid_binop_values, 2,
				    av, bv);
	    RETURN (Void);
	}
	if (aborting)
	    RETURN (Void);
	ret = (*rep->binary[operator]) (av, bv, 1);
	rep = ValueRep(ret);
	if (rep->reduce)
	    ret = (*rep->reduce) (ret);
	RETURN (ret);
    }
}

Value
UnaryOperate (Value v, UnaryOp operator)
{
    ENTER ();
    Value	ret;
    ValueRep	*rep = ValueRep(v);
    
    if (!rep->unary[operator])
    {
	RaiseStandardException (exception_invalid_unop_value, 1,
				v);
	RETURN (Void);
    }
    if (aborting)
	RETURN (Void);
    ret = (*rep->unary[operator])(v, 1);
    rep = ValueRep(ret);
    if (rep->reduce)
	ret = (*rep->reduce) (ret);
    RETURN (ret);
}

Value
Reduce (Value v)
{
    ValueRep	*rep = ValueRep(v);
    if (rep->reduce)
	v = (*rep->reduce) (v);
    return v;
}

Value
NumericDiv (Value av, Value bv, int expandOk)
{
    ENTER ();

    av = Divide (av, bv);
    if (Negativep (bv))
	av = Ceil (av);
    else
	av = Floor (av);
    RETURN (av);
}

Value
NumericMod (Value av, Value bv, int expandOk)
{
    ENTER ();
    Value   q;

    q = NumericDiv (av, bv, expandOk);
    av = Minus (av, Times (q, bv));
    RETURN (av);
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
	RaiseStandardException (exception_invalid_unop_value, 1, av);
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
	RaiseStandardException (exception_invalid_binop_values, 2,
				av, bv);
	RETURN (Void);
    }
    switch (ValueTag(bv)) {
    case rep_int:
	{
	    Value	p;
	    int		i;
	    int		flip = 0;

	    i = ValueInt(bv);
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

	    i = IntegerMag(bv);
	    if (IntegerSign(bv) == Negative)
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
	RaiseStandardException (exception_invalid_binop_values, 2,
				av, bv);
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
	RaiseStandardException (exception_invalid_binop_values, 2,
				av, bv);
	RETURN (Void);
    }
    if (Negativep (bv))
	RETURN (ShiftR(av, Negate (bv)));
    if (Zerop (bv))
	RETURN(av);
    if (ValueIsInt(bv))
    {
	Sign	sign = Positive;
	int	b = ValueInt(bv);
	
	if (ValueIsInt (av) && b < NICKLE_INT_BITS)
	{
	    signed_digit    rd = (signed_digit) ValueInt (av) << b;
	    
	    if (rd > (signed_digit) MAX_NICKLE_INT || rd < (signed_digit) MIN_NICKLE_INT)
		av = NewSignedDigitInteger (rd);
	    else
		av = NewInt ((int) rd);
	}
	else
	{
	    if (Negativep (av))
		sign = Negative;
	    av = Reduce (NewInteger (sign,
				     NaturalLsl (IntegerMag(IntegerRep.promote (av,0)),
						 ValueInt(bv))));
	}
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
	RaiseStandardException (exception_invalid_binop_values, 2,
				av, bv);
	RETURN (Void);
    }
    if (Negativep (bv))
	RETURN (ShiftL(av, Negate (bv)));
    if (Zerop (bv))
	RETURN(av);
    if (ValueIsInt(bv))
    {
	Sign	sign = Positive;
	int	b = ValueInt(bv);
	
	if (ValueIsInt (av) && b < NICKLE_INT_BITS)
	{
	    av = NewInt (ValueInt (av) >> b);
	}
	else
	{
	    if (Negativep (av))
	    {
		av = Minus (av, Minus (ShiftL (One, bv), One));
		sign = Negative;
	    }
	    av = Reduce (NewInteger (sign,
				     NaturalRsl (IntegerMag(IntegerRep.promote (av,0)),
						 b)));
	}
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
	RaiseStandardException (exception_invalid_binop_values, 2,
				av, bv);
	RETURN (Void);
    }
    RETURN (Reduce (NewInteger (Positive, 
				NaturalGcd (IntegerMag(IntegerRep.promote (av, 0)),
					    IntegerMag(IntegerRep.promote (bv, 0))))));
}

#ifdef GCD_DEBUG
Value
Bdivmod (Value av, Value bv)
{
    ENTER ();
    
    if (!Integralp (ValueTag(av)) || !Integralp (ValueTag(bv)))
    {
	RaiseStandardException (exception_invalid_binop_values, 2,
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
	RaiseStandardException (exception_invalid_binop_values, 2,
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
Print (Value f, Value v, char format, int base, int width, int prec, int fill)
{
    int		i;
    Bool	ret;
    ValueRep	*rep;
    
    if (!v)
    {
	FilePuts (f, "<uninit>");
	return True;
    }
    rep = ValueRep(v);
    if (!rep->print)
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
    ret = (*rep->print) (f, v, format, base, width, prec, fill);
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
	if (!v->array.resizable && ArrayValueBox(&v->array,0)->constant)
	    RETURN (v);
	nv = NewArray (False, v->array.resizable, ArrayType(&v->array),
		       v->array.ndim, ArrayDims(&v->array));
	for (i = 0; i < v->array.ndim; i++)
	    ArrayLimits(&nv->array)[i] = ArrayLimits(&v->array)[i];
	if (v->array.resizable)
	{
	    BoxPtr  *o, *n;
	    int	    l = ArrayNvalues (&v->array);
	    o = BoxVectorBoxes (v->array.u.resize);
	    n = BoxVectorBoxes (nv->array.u.resize);
	    for (i = 0; i < l; i++)
	    {
		BoxValueSet (*n, 0, Copy (BoxValueGet (*o, 0)));
		n++;
		o++;
	    }
	    RETURN(nv);
	}
	else
	{
	    box = v->array.u.fix;
	    nbox = nv->array.u.fix;
	    n = ArrayNvalues (&v->array);
	}
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
    case rep_hash:
	RETURN (HashCopy (v));
    default:
	RETURN (v);
    }
    for (i = 0; i < n; i++)
	BoxValueSet (nbox, i, Copy (BoxValueGet (box, i)));
    RETURN (nv);
}

Value
ValueEqual (Value a, Value b, int expandOk)
{
    return a == b ? TrueVal : FalseVal;
}

Value
ValueHash (Value v)
{
    ValueRep	*rep;

    if (!v)
	return Zero;
    rep = ValueRep(v);
    if (!rep->hash)
	return Zero;
    return NewInt ((*rep->hash) (v) & MAX_NICKLE_INT);
}

static Value
UnitEqual (Value av, Value bv, int expandOk)
{
    return TrueVal;
}

static Bool
UnitPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    FilePuts (f, "<>");
    return True;
}

ValueRep UnitRep = {
    { 0, 0, "UnitRep" },	    /* data */
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
BoolPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    FilePuts (f, av == TrueVal ? "true" : "false");
    return True;
}

ValueRep BoolRep = {
    { 0, 0, "BoolRep" },	    /* data */
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
DataCacheMark (void *object)
{
    DataCache	*dc = object;

    memset (DataCacheValues (dc), '\0', sizeof (void *) * dc->size);
}

static DataType DataCacheType = { DataCacheMark, 0, "DataCacheType" };

DataCachePtr 
NewDataCache (int size)
{
    ENTER ();
    DataCachePtr   dc;
    dc = (DataCachePtr) MemAllocate (&DataCacheType, 
				      sizeof (DataCache) +
				      size * sizeof (void *));
    dc->size = size;
    memset (DataCacheValues(dc), '\0', size * sizeof (Value));
    MemAddRoot (dc);
    RETURN (dc);
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
    if (!HashInit ())
	return 0;
    if (!IntInit ())
	return 0;
    if (!NaturalInit ())
	return 0;
    if (!IntegerInit ())
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
    if (!ForeignInit ())
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
