/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

# define sign(i)	((i) < 0 ? Negative : Positive)

#define INT_CACHE_SIZE	8209

static ValueCachePtr	intCache;

static Value	NewIntReal (int value);
    
Value /* INLINE */
NewInt (int i)
{
    ENTER ();
    unsigned	c = ((unsigned) i) % INT_CACHE_SIZE;
    Value	*ve = &ValueCacheValues(intCache)[c];
    Value	v = *ve;
    
    if (!v || v->ints.value != i)
    {
	v = NewIntReal (i);
	*ve = v;
    }
    RETURN (v);
}

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
	RaiseStandardException (exception_divide_by_zero,
				"int divide by zero",
				2, av, bv);
	RETURN (Void);
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
	RaiseStandardException (exception_divide_by_zero,
				"int div by zero",
				2, av, bv);
	RETURN (Void);
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
	RaiseStandardException (exception_divide_by_zero,
				"int modulus by zero",
				2, av, bv);
	RETURN (Void);
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
	return TrueVal;
    return FalseVal;
}

static Value
IntLess (Value av, Value bv, int expandOk)
{
    int		a = av->ints.value, b = bv->ints.value;
    if (a < b)
	return TrueVal;
    return FalseVal;
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
    int	    a = av->ints.value;
    int	    digit;
    int	    w;
    int	    fraction_width;
    char    space[64], *s;
    char    letter;
    int	    neg;

    if ('A' <= format && format <= 'Z')
	letter = 'A';
    else
	letter = 'a';
    if (base == 0)
	base = 10;
    switch (format) {
    case 'c':
	space[StringPutChar (a, space)] = '\0';
	s = space;
	break;
    default:
	s = space + sizeof (space);
	*--s = '\0';
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
    w = StringLength (s);
    fraction_width = 0;
    if (prec >= 0)
    {
	int avail_width;
	
	if (width > 0)
	    avail_width = width;
	else
	    avail_width = -width;
        fraction_width = prec + 1;
	if (avail_width > 0)
	{
	    if (w + fraction_width > avail_width)
	    {
		fraction_width = avail_width - w;
		if (fraction_width < 0)
		    fraction_width = 0;
	    }
	}
    }
    w += fraction_width;
    while (width > w)
    {
	FileOutput (f, fill);
	width--;
    }
    FilePuts (f, s);
    if (fraction_width)
    {
	FileOutput (f, '.');
	--fraction_width;
	while (fraction_width)
	{
	    FileOutput (f, '0');
	    --fraction_width;
	}
    }
    while (-width > w)
    {
	FileOutput (f, fill);
	width++;
    }
    return True;
}

ValueRep IntRep = {
    { 0, 0 },	    /* data */
    rep_int,	    /* tag */
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
    
static Value
NewIntReal (int value)
{
    ENTER ();
    Value	ret;
    ret = ALLOCATE (&IntRep.data, sizeof (Int));
    ret->ints.value = value;
    RETURN (ret);
}

int
IntInit (void)
{
    ENTER ();
    intCache = NewValueCache(INT_CACHE_SIZE);
    Zero = NewInt (0);
    MemAddRoot (Zero);
    One = NewInt (1);
    MemAddRoot (One);
    EXIT ();
    return 1;
}
