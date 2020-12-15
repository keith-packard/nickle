/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

static Value
IntPlus (Value av, Value bv, int expandOk)
{
    int	    r = ValueInt(av) + ValueInt(bv);
    
    if (expandOk && NICKLE_INT_CARRIED(r))
	return Plus (NewIntInteger (ValueInt(av)), NewIntInteger(ValueInt(bv)));
    return NewInt(r);
}

static Value
IntMinus (Value av, Value bv, int expandOk)
{
    int	    r = ValueInt(av) - ValueInt(bv);
    
    if (expandOk && NICKLE_INT_CARRIED(r))
	return Minus (NewIntInteger (ValueInt(av)), NewIntInteger(ValueInt(bv)));
    return NewInt(r);
}

int
logbase2(int a)
{
    int log = 0;

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
		
#define HALF_BITS   (NICKLE_INT_BITS>>1)
#define HALF_MAX    (1 << (NICKLE_INT_BITS>>1))

static Value
IntTimes (Value av, Value bv, int expandOk)
{
    int		    a = ValueInt(av), b = ValueInt(bv);
    signed_digit    rd = (signed_digit) a * (signed_digit) b;

    if (rd > (signed_digit) MAX_NICKLE_INT || rd < (signed_digit) MIN_NICKLE_INT)
	return NewSignedDigitInteger (rd);
    return NewInt ((int) rd);
}

static Value
IntDivide (Value av, Value bv, int expandOk)
{
    ENTER ();
    int		a = ValueInt(av), b = ValueInt(bv);
    Value	ret;

    if (b == 0)
    {
	RaiseStandardException (exception_divide_by_zero, 2,
				av, bv);
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
    int		a = ValueInt(av), b = ValueInt(bv);
    int		d;
    Value	ret;

    if (b == 0)
    {
	RaiseStandardException (exception_divide_by_zero, 2,
				av, bv);
	RETURN (Void);
    }
    switch (catagorize_signs (IntSign(a), IntSign(b))) {
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
 * IntSign(quotient) = IntSign (dividend) * IntSign (divisor)
 * 0 <= remainder < abs (dividend)
 */
 
static Value
IntMod (Value av, Value bv, int expandOk)
{
    ENTER ();
    int		a = ValueInt(av), b = ValueInt(bv);
    int		r;

    if (b == 0)
    {
	RaiseStandardException (exception_divide_by_zero, 2,
				av, bv);
	RETURN (Void);
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
    RETURN (NewInt (r));
}

static Value
IntEqual (Value av, Value bv, int expandOk)
{
    int		a = ValueInt(av), b = ValueInt(bv);
    if (a == b)
	return TrueVal;
    return FalseVal;
}

static Value
IntLess (Value av, Value bv, int expandOk)
{
    int		a = ValueInt(av), b = ValueInt(bv);
    if (a < b)
	return TrueVal;
    return FalseVal;
}

static Value
IntLand (Value av, Value bv, int expandOk)
{
    ENTER ();
    int		a = ValueInt(av), b = ValueInt(bv);
    RETURN (NewInt (a & b));
}

static Value
IntLor (Value av, Value bv, int expandOk)
{
    ENTER ();
    int		a = ValueInt(av), b = ValueInt(bv);
    RETURN (NewInt (a | b));
}

static Value
IntNegate (Value av, int expandOk)
{
    ENTER ();
    int	    a = ValueInt(av);

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
IntPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    int	    a = ValueInt(av);
    int	    digit;
    int	    w;
    int	    fraction_width;
    char    space[64], *s;
    char    letter;
    int	    neg;
    long    len;

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
    len = strlen (s);
    w = StringLength (s, len);
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
	FileOutchar (f, fill);
	width--;
    }
    FilePutsc (f, s, len);
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
	FileOutchar (f, fill);
	width++;
    }
    return True;
}

static HashValue
IntHash (Value av)
{
    return (HashValue) ValueInt (av);;
}

ValueRep IntRep = {
    { 0, 0, "IntRep" },	    /* data */
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
    IntPrint,
    0,
    IntHash,
};

int
IntInit (void)
{
    return 1;
}
