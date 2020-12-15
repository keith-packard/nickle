/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * integer.c
 *
 * operations on integers
 */

#include	"nickle.h"

#define IMag(i)	((Natural *) ((long) ((i)->magn) & ~1))
#define ISign(i)	((Sign) ((long) ((i)->magn) & 1))

int
IntegerToInt (Integer *i)
{
    int	result;

    result = NaturalToInt (IMag(i));
    if (ISign(i) == Negative)
	result = -result;
    return result;
}

signed_digit
IntegerToSignedDigit(Integer *i)
{
    double_digit	dd;
    signed_digit	sd;

    dd = NaturalToDoubleDigit(IMag(i));
    if (ISign(i) == Negative)
	sd = -(signed_digit) dd;
    else
	sd = (signed_digit) dd;
    return sd;
}

int
IntegerFitsSignedDigit(Integer *i)
{
    return NaturalLess(IMag(i), max_signed_digit_natural);
}

static Value
IntegerPlus (Value av, Value bv, int expandOk)
{
    ENTER ();
    Integer	*a = &av->integer, *b = &bv->integer;
    Value	ret;

    switch (catagorize_signs(ISign(a), ISign(b))) {
    case BothPositive:
    default:
	ret = NewInteger (Positive, NaturalPlus (IMag(a), IMag(b)));
	break;
    case FirstPositive:
	if (NaturalLess (IMag(a), IMag(b)))
	    ret = NewInteger (Negative, NaturalMinus (IMag(b), IMag(a)));
	else
	    ret = NewInteger (Positive, NaturalMinus (IMag(a), IMag(b)));
	break;
    case SecondPositive:
	if (NaturalLess (IMag(a), IMag(b)))
	    ret = NewInteger (Positive, NaturalMinus (IMag(b), IMag(a)));
	else
	    ret = NewInteger (Negative, NaturalMinus (IMag(a), IMag(b)));
	break;
    case BothNegative:
	ret = NewInteger (Negative, NaturalPlus (IMag(a), IMag(b)));
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

    switch (catagorize_signs(ISign(a), ISign(b))) {
    case BothPositive:
    default:
	if (NaturalLess (IMag(a), IMag(b)))
	    ret = NewInteger (Negative, NaturalMinus (IMag(b), IMag(a)));
	else
	    ret = NewInteger (Positive, NaturalMinus (IMag(a), IMag(b)));
	break;
    case FirstPositive:
	ret = NewInteger (Positive, NaturalPlus (IMag(a), IMag(b)));
	break;
    case SecondPositive:
	ret = NewInteger (Negative, NaturalPlus (IMag(a), IMag(b)));
	break;
    case BothNegative:
	if (NaturalLess (IMag(a), IMag(b)))
	    ret = NewInteger (Positive, NaturalMinus (IMag(b), IMag(a)));
	else
	    ret = NewInteger (Negative, NaturalMinus (IMag(a), IMag(b)));
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
    if (ISign(a) != ISign(b))
	sign = Negative;
    RETURN (NewInteger (sign, NaturalTimes (IMag(a), IMag(b))));
}

static Value
IntegerDivide (Value av, Value bv, int expandOk)
{
    ENTER ();
    Integer	*a = &av->integer, *b = &bv->integer;
    Natural	*rem;
    Sign	sign;

    if (NaturalZero (IMag(b)))
    {
	RaiseStandardException (exception_divide_by_zero, 2,
				av, bv);
	RETURN (Void);
    }
    sign = Positive;
    if (ISign(a) != ISign(b))
	sign = Negative;
    if (expandOk)
	RETURN (NewRational (sign, IMag(a), IMag(b)));
    else
	RETURN (NewInteger (sign, NaturalDivide (IMag(a), IMag(b), &rem)));
}

static Value
IntegerDiv (Value av, Value bv, int expandOk)
{
    ENTER ();
    Integer	*a = &av->integer, *b = &bv->integer;
    Sign	sign;
    Natural	*quo, *rem;

    if  (NaturalZero (IMag(b)))
    {
	RaiseStandardException (exception_divide_by_zero, 2,
				av, bv);
	RETURN (Void);
    }
    quo = NaturalDivide (IMag(a), IMag(b), &rem);
    sign = Positive;
    if (ISign (a) != ISign (b))
	sign = Negative;
    if (ISign (a) == Negative && !NaturalZero (rem))
        quo = NaturalPlus (quo, one_natural);
    RETURN (NewInteger (sign, quo));
}

static Value
IntegerMod (Value av, Value bv, int expandOk)
{
    ENTER ();
    Integer	*a = &av->integer, *b = &bv->integer;
    Natural	*rem;

    if  (NaturalZero (IMag(b)))
    {
	RaiseStandardException (exception_divide_by_zero, 2,
				av, bv);
	RETURN (Void);
    }
    (void) NaturalDivide (IMag(a), IMag(b), &rem);
    if (ISign(a) == Negative && !NaturalZero (rem))
	rem = NaturalMinus (IMag(b), rem);
    RETURN (NewInteger (Positive, rem));
}

static Value
IntegerLess (Value av, Value bv, int expandOk)
{
    Integer	*a = &av->integer, *b = &bv->integer;
    Value	ret;

    ret = FalseVal;
    switch (catagorize_signs (ISign(a), ISign(b))) {
    case BothPositive:
	if (NaturalLess (IMag(a), IMag(b)))
	    ret = TrueVal;
	break;
    case FirstPositive:
	break;
    case SecondPositive:
	ret = TrueVal;
	break;
    case BothNegative:
	if (NaturalLess (IMag(b), IMag(a)))
	    ret = TrueVal;
	break;
    }
    return ret;
}

static Value
IntegerEqual (Value av, Value bv, int expandOk)
{
    Integer	*a = &av->integer, *b = &bv->integer;

    if (ISign(a) == ISign(b) && NaturalEqual (IMag(a), IMag(b)))
	return TrueVal;
    return FalseVal;
}

#if 0
#define DebugN(s,n) FilePrintf (FileStdout, "%s %N\n", s, n)
#else
#define DebugN(s,n)
#endif

static Value
IntegerLand (Value av, Value bv, int expandOk)
{
    ENTER ();
    Value	ret;
    Integer	*a = &av->integer, *b = &bv->integer;
    Natural	*am = IMag(a), *bm = IMag(b), *m;

    DebugN("a", am);
    if (ISign(a) == Negative)
    {
	am = NaturalNegate (am, NaturalLength (bm));
	DebugN ("-a", am);
    }
    DebugN("b", bm);
    if (ISign(b) == Negative)
    {
	bm = NaturalNegate (bm, NaturalLength (am));
	DebugN("-b", bm);
    }
    m = NaturalLand (am, bm);
    DebugN("m", m);
    if (ISign(a) == Negative && ISign(b) == Negative)
    {
	m = NaturalNegate (m, 0);
	DebugN("-m", m);
	ret = NewInteger (Negative, m);
    }
    else
	ret = NewInteger (Positive, m);
    RETURN (ret);
}


static Value
IntegerLor (Value av, Value bv, int expandOk)
{
    ENTER ();
    Value	ret;
    Integer	*a = &av->integer, *b = &bv->integer;
    Natural	*am = IMag(a), *bm = IMag(b), *m;

    DebugN("a", am);
    if (ISign(a) == Negative)
    {
	am = NaturalNegate (am, NaturalLength (bm));
	DebugN ("-a", am);
    }
    DebugN("b", bm);
    if (ISign(b) == Negative)
    {
	bm = NaturalNegate (bm, NaturalLength (am));
	DebugN("-b", bm);
    }
    m = NaturalLor (am, bm);
    DebugN("m", m);
    if (ISign(a) == Negative || ISign(b) == Negative)
    {
	m = NaturalNegate (m, 0);
	DebugN("-m", m);
	ret = NewInteger (Negative, m);
    }
    else
	ret = NewInteger (Positive, m);
    RETURN (ret);
}

static Value
IntegerNegate (Value av, int expandOk)
{
    Integer *a = &av->integer;

    return NewInteger (SignNegate (ISign(a)), IMag(a));
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
IntegerPromote (Value av, Value bv)
{
    if (ValueIsInt(av))
	av = NewIntInteger (ValueInt(av));
    return av;
}

static Value
IntegerReduce (Value av)
{
    Integer *a = &av->integer;

    if (NaturalLess (IMag(a), max_int_natural))
	av = NewInt (IntegerToInt (a));
    return av;
}

static Bool
IntegerPrint (Value f, Value iv, char format, int base, int width, int prec, int fill)
{
    ENTER ();
    Integer *i = &iv->integer;
    char    *result;
    int	    print_width;
    int	    fraction_width;

    if (base == 0)
	base = 10;
    result = NaturalSprint (0, IMag(i), base, &print_width);
    if (result)
    {
	if (ISign(i) == Negative)
	    print_width++;
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
		if (print_width + fraction_width > avail_width)
		{
		    fraction_width = avail_width - print_width;
		    if (fraction_width < 0)
			fraction_width = 0;
		}
	    }
	}
	print_width += fraction_width;
	while (width > print_width)
	{
	    FileOutchar (f, fill);
	    width--;
	}
	if (ISign(i) == Negative)
	    FileOutput (f, '-');
	FilePuts (f, result);
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
	while (-width > print_width)
	{
	    FileOutchar (f, fill);
	    width++;
	}
    }
    EXIT ();
    return result != 0;
}

static HashValue
IntegerHash (Value iv)
{
    return NaturalHash (IntegerMag(iv)) ^ IntegerSign(iv);
}

static void
IntegerMark (void *object)
{
    Integer *integer = object;
    MemReference (IMag(integer));
}

ValueRep    IntegerRep = {
    { IntegerMark, 0, "IntegerRep" },	    /* base */
    rep_integer,	    /* tag */
    {			    /* binary */
	IntegerPlus,
	IntegerMinus,
	IntegerTimes,
	IntegerDivide,
	IntegerDiv,
	IntegerMod,
	IntegerLess,
	IntegerEqual,
	IntegerLand,
	IntegerLor,
    },
    {			    /* unary */
	IntegerNegate,
	IntegerFloor,
	IntegerCeil,
    },
    IntegerPromote,
    IntegerReduce,
    IntegerPrint,
    0,
    IntegerHash,
};

#define INTEGER_CACHE_SIZE  8191

DataCachePtr	integerCache;

Value
NewInteger (Sign sign, Natural *mag)
{
    ENTER ();
    unsigned	c = (PtrToUInt(mag) ^ (unsigned) sign) % INTEGER_CACHE_SIZE;
    Value	*re = (Value *) DataCacheValues(integerCache) + c;
    Value	ret = *re;

    if (ret && IntegerSign(ret) == sign && NaturalEqual (mag, IntegerMag(ret)))
    {
	RETURN (ret);
    }
    ret = ALLOCATE (&IntegerRep.data, sizeof (Integer));
    ret->integer.magn = (Natural *) (((long) mag) | (long) sign);
    *re = ret;
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
NewSignedDigitInteger (signed_digit d)
{
    ENTER ();
    Sign	    sign = Positive;
    double_digit    dd;

    if (d < 0)
    {
	sign = Negative;
	dd = -d;
    }
    else
	dd = d;
    RETURN (NewInteger (sign, NewDoubleDigitNatural (dd)));
}

int
IntegerInit (void)
{
    ENTER ();

    integerCache = NewDataCache (INTEGER_CACHE_SIZE);
    EXIT ();
    return 1;
}
