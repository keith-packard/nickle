/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"value.h"

# define sign(i)	((i) < 0 ? Negative : Positive)

Value	One, Zero;

static Value
IntPlus (Value av, Value bv, int expandOk)
{
    ENTER ();
    Value	ret;
    int		a = av->ints.value, b = bv->ints.value;

    switch (catagorize_signs (sign(a), sign(b))) {
    case BothPositive:
	if (expandOk && a + b < 0)
	    ret = Plus (NewIntInteger (a), NewIntInteger (b));
	else
	    ret = NewInt (a + b);
	break;
    case FirstPositive:
    case SecondPositive:
    default:
	ret = NewInt (a + b);
	break;
    case BothNegative:
	if (expandOk && a + b >= 0)
	    ret = Plus (NewIntInteger (a), NewIntInteger (b));
	else
	    ret = NewInt (a + b);
	break;
    }
    RETURN (ret);
}

static Value
IntMinus (Value av, Value bv, int expandOk)
{
    ENTER ();
    Value	ret;
    int		a = av->ints.value, b = bv->ints.value;

    switch (catagorize_signs (sign(a), sign(b))) {
    case FirstPositive:
	if (expandOk && a - b < 0)
	    ret = Minus (NewIntInteger (a), NewIntInteger (b));
	else
	    ret = NewInt (a - b);
	break;
    case BothPositive:
    case BothNegative:
    default:
	ret = NewInt (a - b);
	break;
    case SecondPositive:
	if (expandOk && a - b >= 0)
	    ret = Minus (NewIntInteger (a), NewIntInteger (b));
	else
	    ret = NewInt (a - b);
	break;
    }
    RETURN (ret);
}

static int
logbase2(int a)
{
    int	log = 0;

    if (a < 0)
	a = -a;
    while (a & (~ 0xff)) {
	log += 8;
	a >>= 8;
    }
    if (a & (~0xf)) {
	log += 4;
	a >>= 4;
    }
    if (a & (~0x3)) {
	log += 2;
	a >>= 2;
    }
    if (a & (~0x1))
	log += 1;
    return log;
}

static Value
IntTimes (Value av, Value bv, int expandOk)
{
    ENTER ();
    int		a = av->ints.value, b = bv->ints.value;
    Value	ret;

    if (expandOk && logbase2(a) + logbase2(b) >= logbase2 (MAXINT))
	ret = Times (NewIntInteger (a), NewIntInteger (b));
    else
	ret = NewInt (a * b);
    RETURN (ret);
}

static Value
IntDivide (Value av, Value bv, int expandOk)
{
    ENTER ();
    int		a = av->ints.value, b = bv->ints.value;
    Value	ret;

    if (b == 0)
    {
	RaiseError ("int division by zero %v/%v", av, bv);
	RETURN (Zero);
    }
    if (expandOk && a % b != 0)
	ret = Divide (NewIntRational (a), NewIntRational (b));
    else
	ret = NewInt (a/b);
    RETURN (ret);
}

static Value
IntDiv (Value av, Value bv, int expandOk)
{
    ENTER ();
    int		a = av->ints.value, b = bv->ints.value;
    int		d;
    Value	ret;

    if (b == 0)
    {
	RaiseError ("int div by zero %v/%v", av, bv);
	RETURN (Zero);
    }
    switch (catagorize_signs (sign(a), sign(b))) {
    case BothPositive:
	d = a / b;
	break;
    case FirstPositive:
	d = - (a / -b);
	break;
    case SecondPositive:
	d = -(a / -b);
	if (a % -b)
	    d--;
	break;
    case BothNegative:
    default:
	d = -a / -b;
	if (-a % -b)
	    d++;
	break;
    }
    ret = NewInt (d);
    RETURN (ret);
}

/*
 * dividend * quotient + remainder = divisor
 *
 * sign(quotient) = sign (dividend) * sign (divisor)
 * 0 <= remainder < abs (dividend)
 */
 
static Value
IntMod (Value av, Value bv, int expandOk)
{
    ENTER ();
    int		a = av->ints.value, b = bv->ints.value;
    int		r;

    if (b == 0)
    {
	RaiseError ("int modulus by zero %v %% %v", av, bv);
	RETURN (Zero);
    }
    switch (catagorize_signs (sign(a), sign(b))) {
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
    RETURN (NewInt (r));
}

static Value
IntEqual (Value av, Value bv, int expandOk)
{
    int		a = av->ints.value, b = bv->ints.value;
    if (a == b)
	return One;
    return Zero;
}

static Value
IntLess (Value av, Value bv, int expandOk)
{
    int		a = av->ints.value, b = bv->ints.value;
    if (a < b)
	return One;
    return Zero;
}

static Value
IntLand (Value av, Value bv, int expandOk)
{
    ENTER ();
    int		a = av->ints.value, b = bv->ints.value;
    RETURN (NewInt (a & b));
}

static Value
IntLor (Value av, Value bv, int expandOk)
{
    ENTER ();
    int		a = av->ints.value, b = bv->ints.value;
    RETURN (NewInt (a | b));
}

static Value
IntNegate (Value av, int expandOk)
{
    ENTER ();
    int	    a = av->ints.value;

    if (-(-a) != a)
	RETURN (Negate (NewIntInteger (a)));
    RETURN (NewInt (-a));
}

static Value
IntFloor (Value av, int expandOk)
{
    return av;
}

static Value
IntCeil (Value av, int expandOk)
{
    return av;
}

static Bool
IntPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    int		    a = av->ints.value;
    int		    digit;
    int		    w;
    unsigned char   space[64], *s;
    unsigned char   letter;
    int		    neg;

    if ('A' <= format && format <= 'Z')
	letter = 'A';
    else
	letter = 'a';
    if (base == 0)
	base = 10;
    s = space + sizeof (space);
    *--s = '\0';
    switch (format) {
    case 'c':
	*--s = a & 0xff;
	break;
    default:
	neg = 0;
	if (a < 0)
	{
	    a = -a;
	    neg = 1;
	}
	if (!a)
	    *--s = '0';
	else
	{
	    while (a)
	    {
		digit = a % base;
		if (digit <= 9) 
		    digit = '0' + digit;
		else
		    digit = letter + digit - 10;
		*--s = digit;
		a /= base;
	    }
	    if (neg)
		*--s = '-';
	}
    }
    w = (space + sizeof (space) - s) - 1;
    while (width > w)
    {
	FileOutput (f, fill);
	width--;
    }
    FilePuts (f, s);
    while (-width > w)
    {
	FileOutput (f, fill);
	width++;
    }
    return True;
}

ValueType IntType = {
    { 0, 0 },	    /* data */
    {		    /* binary */
	IntPlus,
	IntMinus,
	IntTimes,
	IntDivide,
	IntDiv,
	IntMod,
	IntLess,
	IntEqual,
	IntLand,
	IntLor
    },
    {
	IntNegate,
	IntFloor,
	IntCeil,
    },
    0, 0,
    IntPrint
};
    
Value
NewInt (int value)
{
    ENTER ();
    Value	ret;

    ret = ALLOCATE (&IntType.data, sizeof (Int));
    ret->value.tag = type_int;
    ret->ints.value = value;
    RETURN (ret);
}

int
IntInit (void)
{
    ENTER ();
    Zero = NewInt (0);
    MemAddRoot (Zero);
    One = NewInt (1);
    MemAddRoot (One);
    EXIT ();
    return 1;
}
