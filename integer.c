/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * integer.c
 *
 * operations on integers
 */

#include	<config.h>

#include	"value.h"

int
IntegerToInt (Integer *i)
{
    int	result;

    result = NaturalToInt (i->mag);
    if (i->sign == Negative)
	result = -result;
    return result;
}

static Value
IntegerPlus (Value av, Value bv, int expandOk)
{
    ENTER ();
    Integer	*a = &av->integer, *b = &bv->integer;
    Value	ret;

    switch (catagorize_signs(a->sign, b->sign)) {
    case BothPositive:
    default:
	ret = NewInteger (Positive, NaturalPlus (a->mag, b->mag));
	break;
    case FirstPositive:
	if (NaturalLess (a->mag, b->mag))
	    ret = NewInteger (Negative, NaturalMinus (b->mag, a->mag));
	else
	    ret = NewInteger (Positive, NaturalMinus (a->mag, b->mag));
	break;
    case SecondPositive:
	if (NaturalLess (a->mag, b->mag))
	    ret = NewInteger (Positive, NaturalMinus (b->mag, a->mag));
	else
	    ret = NewInteger (Negative, NaturalMinus (a->mag, b->mag));
	break;
    case BothNegative:
	ret = NewInteger (Negative, NaturalPlus (a->mag, b->mag));
	break;
    }
    RETURN (ret);
}

static Value
IntegerMinus (Value av, Value bv, int expandOk)
{
    ENTER ();
    Integer	*a = &av->integer, *b = &bv->integer;
    Value	ret;

    switch (catagorize_signs(a->sign, b->sign)) {
    case BothPositive:
    default:
	if (NaturalLess (a->mag, b->mag))
	    ret = NewInteger (Negative, NaturalMinus (b->mag, a->mag));
	else
	    ret = NewInteger (Positive, NaturalMinus (a->mag, b->mag));
	break;
    case FirstPositive:
	ret = NewInteger (Positive, NaturalPlus (a->mag, b->mag));
	break;
    case SecondPositive:
	ret = NewInteger (Negative, NaturalPlus (a->mag, b->mag));
	break;
    case BothNegative:
	if (NaturalLess (a->mag, b->mag))
	    ret = NewInteger (Positive, NaturalMinus (b->mag, a->mag));
	else
	    ret = NewInteger (Negative, NaturalMinus (a->mag, b->mag));
	break;
    }
    RETURN (ret);
}

static Value
IntegerTimes (Value av, Value bv, int expandOk)
{
    ENTER ();
    Integer	*a = &av->integer, *b = &bv->integer;
    Sign	sign;
    
    sign = Positive;
    if (a->sign != b->sign)
	sign = Negative;
    RETURN (NewInteger (sign, NaturalTimes (a->mag, b->mag)));
}

static Value
IntegerDivide (Value av, Value bv, int expandOk)
{
    ENTER ();
    Integer	*a = &av->integer, *b = &bv->integer;
    Natural	*rem;
    Sign	sign;
    
    if (NaturalZero (b->mag))
    {
	RaiseError ("integer divide by zero %v / %v", av, bv);
	RETURN (Zero);
    }
    sign = Positive;
    if (a->sign != b->sign)
	sign = Negative;
    if (expandOk)
	RETURN (NewRational (sign, a->mag, b->mag));
    else
	RETURN (NewInteger (sign, NaturalDivide (a->mag, b->mag, &rem)));
}

static Value
IntegerDiv (Value av, Value bv, int expandOk)
{
    ENTER ();
    Integer	*a = &av->integer, *b = &bv->integer;
    Sign	sign;
    Natural	*quo, *rem;
    
    if  (NaturalZero (b->mag))
    {
	RaiseError ("integer div by zero %v %% %v", av,  bv);
	RETURN (Zero);
    }
    quo = NaturalDivide (a->mag, b->mag, &rem);
    sign = Positive;
    if (a->sign == Positive != b->sign == Positive)
	sign = Negative;
    if (b->sign == Negative && !NaturalZero (rem))
	quo = NaturalPlus (quo, one_natural);
    RETURN (NewInteger (sign, quo));
}

static Value
IntegerMod (Value av, Value bv, int expandOk)
{
    ENTER ();
    Integer	*a = &av->integer, *b = &bv->integer;
    Natural	*quo, *rem;
    
    if  (NaturalZero (b->mag))
    {
	RaiseError ("integer modulus by zero %v %% %v", av,  bv);
	RETURN (Zero);
    }
    quo = NaturalDivide (a->mag, b->mag, &rem);
    if (a->sign == Negative && !NaturalZero (rem))
	rem = NaturalMinus (b->mag, rem);
    RETURN (NewInteger (Positive, rem));
}

static Value
IntegerLess (Value av, Value bv, int expandOk)
{
    Integer	*a = &av->integer, *b = &bv->integer;
    Value	ret;

    ret = Zero;
    switch (catagorize_signs (a->sign, b->sign)) {
    case BothPositive:
	if (NaturalLess (a->mag, b->mag))
	    ret = One;
	break;
    case FirstPositive:
	break;
    case SecondPositive:
	ret = One;
	break;
    case BothNegative:
	if (NaturalLess (b->mag, a->mag))
	    ret = One;
	break;
    }
    return ret;
}

static Value
IntegerEqual (Value av, Value bv, int expandOk)
{
    Integer	*a = &av->integer, *b = &bv->integer;

    if (a->sign == b->sign && NaturalEqual (a->mag, b->mag))
	return One;
    return Zero;
}

static Value
IntegerNegate (Value av, int expandOk)
{
    ENTER ();
    Integer *a = &av->integer;

    RETURN (NewInteger (SignNegate (a->sign), a->mag));
}

static Value
IntegerFloor (Value av, int expandOk)
{
    return av;
}

static Value
IntegerCeil (Value av, int expandOk)
{
    return av;
}

static Value
IntegerPromote (Value av)
{
    ENTER ();
    
    switch (av->value.tag) {
    case type_int:
	av = NewIntInteger (av->ints.value);
    default:
	break;
    }	
    RETURN (av);
}

static Value
IntegerReduce (Value av)
{
    ENTER ();
    Integer *a = &av->integer;
    
    if (NaturalLess (a->mag, max_int_natural))
	av = NewInt (IntegerToInt (a));
    RETURN (av);
}

static Bool
IntegerPrint (Value f, Value iv, char format, int base, int width, int prec, unsigned char fill)
{
    ENTER ();
    Integer *i = &iv->integer;
    char    *result;
    int	    print_width;
    
    if (base == 0)
	base = 10;
    result = NaturalSprint (0, i->mag, base, &print_width);
    if (result)
    {
	if (i->sign == Negative)
	    print_width++;
	while (width > print_width)
	{
	    FileOutput (f, fill);
	    width--;
	}
	if (i->sign == Negative)
	    FileOutput (f, '-');
	FilePuts (f, result);
	while (-width > print_width)
	{
	    FileOutput (f, fill);
	    width++;
	}
    }
    EXIT ();
    return result != 0;
}

static void
IntegerMark (void *object)
{
    Integer *integer = object;
    MemReference (integer->mag);
}

ValueType    IntegerType = { 
    { IntegerMark, 0 },	    /* base */
    {			    /* binary */
	IntegerPlus,
	IntegerMinus,
	IntegerTimes,
	IntegerDivide,
	IntegerDiv,
	IntegerMod,
	IntegerLess,
	IntegerEqual,
	0,
	0,
    },
    {			    /* unary */
	IntegerNegate,
	IntegerFloor,
	IntegerCeil,
    },
    IntegerPromote,
    IntegerReduce,
    IntegerPrint
};

Value
NewInteger (Sign sign, Natural *mag)
{
    ENTER ();
    Value ret;

    ret = ALLOCATE (&IntegerType.data, sizeof (Integer));
    ret->value.tag = type_integer;
    ret->integer.sign = sign;
    ret->integer.mag = mag;
    RETURN (ret);
}

Value
NewIntInteger (int i)
{
    ENTER ();
    Sign	    sign = Positive;
    unsigned long   mag;

    if (i < 0)
    {
	sign = Negative;
	mag = -i;
    }
    else
	mag = i;
    RETURN (NewInteger (sign, NewNatural (mag)));
}
	
Value
NewDoubleInteger (double d)
{
    ENTER ();
    Sign	sign;

    if (d < 0)
    {
	sign = Negative;
	d = -d;
    }
    else
	sign = Positive;
    RETURN (NewInteger (sign, NewDoubleNatural (d)));
}
