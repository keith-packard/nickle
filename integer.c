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

#include	"nickle.h"

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
	RaiseStandardException (exception_divide_by_zero,
				"integer divide by zero",
				2, av, bv);
	RETURN (Void);
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
	RaiseStandardException (exception_divide_by_zero,
				"integer div by zero",
				2, av, bv);
	RETURN (Void);
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
	RaiseStandardException (exception_divide_by_zero,
				"integer modulus by zero",
				2, av, bv);
	RETURN (Void);
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

    ret = FalseVal;
    switch (catagorize_signs (a->sign, b->sign)) {
    case BothPositive:
	if (NaturalLess (a->mag, b->mag))
	    ret = TrueVal;
	break;
    case FirstPositive:
	break;
    case SecondPositive:
	ret = TrueVal;
	break;
    case BothNegative:
	if (NaturalLess (b->mag, a->mag))
	    ret = TrueVal;
	break;
    }
    return ret;
}

static Value
IntegerEqual (Value av, Value bv, int expandOk)
{
    Integer	*a = &av->integer, *b = &bv->integer;

    if (a->sign == b->sign && NaturalEqual (a->mag, b->mag))
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
    Natural	*am = a->mag, *bm = b->mag, *m;

    DebugN("a", am);
    if (a->sign == Negative)
    {
	am = NaturalNegate (am, NaturalLength (bm));
	DebugN ("-a", am);
    }
    DebugN("b", bm);
    if (b->sign == Negative)
    {
	bm = NaturalNegate (bm, NaturalLength (am));
	DebugN("-b", bm);
    }
    m = NaturalLand (am, bm);
    DebugN("m", m);
    if (a->sign == Negative && b->sign == Negative)
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
    Natural	*am = a->mag, *bm = b->mag, *m;

    DebugN("a", am);
    if (a->sign == Negative)
    {
	am = NaturalNegate (am, NaturalLength (bm));
	DebugN ("-a", am);
    }
    DebugN("b", bm);
    if (b->sign == Negative)
    {
	bm = NaturalNegate (bm, NaturalLength (am));
	DebugN("-b", bm);
    }
    m = NaturalLor (am, bm);
    DebugN("m", m);
    if (a->sign == Negative || b->sign == Negative)
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
IntegerPromote (Value av, Value bv)
{
    ENTER ();
    
    if (ValueIsInt(av))
	av = NewIntInteger (av->ints.value);
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
IntegerPrint (Value f, Value iv, char format, int base, int width, int prec, int fill)
{
    ENTER ();
    Integer *i = &iv->integer;
    char    *result;
    int	    print_width;
    int	    fraction_width;
    
    if (base == 0)
	base = 10;
    result = NaturalSprint (0, i->mag, base, &print_width);
    if (result)
    {
	if (i->sign == Negative)
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
	if (i->sign == Negative)
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

static void
IntegerMark (void *object)
{
    Integer *integer = object;
    MemReference (integer->mag);
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
    IntegerPrint
};

Value
NewInteger (Sign sign, Natural *mag)
{
    ENTER ();
    Value ret;

    ret = ALLOCATE (&IntegerRep.data, sizeof (Integer));
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
