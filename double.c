/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	<math.h>
#include	"value.h"
#ifdef HAVE_IEEEFP_H
/* finite() on Solaris */
#include <ieeefp.h>
#endif
#ifdef HAS_FPU_CONTROL_H
#include	<fpu_control.h>
#endif

void
ignore_ferr (void)
{
#ifdef _FPU_IEEE
#ifdef HAS___SETFPUCW
    __setfpucw (_FPU_IEEE);
#else
    _FPU_SETCW(_FPU_IEEE);
#endif
#endif
}

Value
DoublePlus (Value av, Value bv, int expandOk)
{
    ENTER ();
    double	a = av->doubles.value, b = bv->doubles.value;

    RETURN (NewDouble (a + b));
}

Value
DoubleMinus (Value av, Value bv, int expandOk)
{
    ENTER ();
    double	a = av->doubles.value, b = bv->doubles.value;

    RETURN (NewDouble (a - b));
}

Value
DoubleTimes (Value av, Value bv, int expandOk)
{
    ENTER ();
    double	a = av->doubles.value, b = bv->doubles.value;

    RETURN (NewDouble (a * b));
}

Value
DoubleDivide (Value av, Value bv, int expandOk)
{
    ENTER ();
    double	a = av->doubles.value, b = bv->doubles.value;

    RETURN (NewDouble (a / b));
}

Value
DoubleMod (Value av, Value bv, int expandOk)
{
    ENTER ();
    double	a = av->doubles.value, b = bv->doubles.value;

    RETURN (NewDouble (((int) a) % ((int) b)));
}

Value
DoubleEqual (Value av, Value bv, int expandOk)
{
    double	a = av->doubles.value, b = bv->doubles.value;

    if (a == b)
	return One;
    return Zero;
}

Value
DoubleLess (Value av, Value bv, int expandOk)
{
    double	a = av->doubles.value, b = bv->doubles.value;

    if (a < b)
	return One;
    return Zero;
}

Value
DoubleNegate (Value av, int expandOk)
{
    ENTER ();
    RETURN (NewDouble (-av->doubles.value));
}

Value
DoubleFloor (Value av, int expandOk)
{
    double  floor (double);
    ENTER ();

    RETURN (NewDoubleInteger (floor (av->doubles.value)));
}

Value
DoubleCeil (Value av, int expandOk)
{
    double  ceil (double);
    ENTER ();
    RETURN (NewDoubleInteger (ceil (av->doubles.value)));
}

Value
DoublePromote (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_int:
	av = NewDouble ((double) av->ints.value);
	break;
    case type_integer:
	av = NewIntegerDouble (&av->integer);
	break;
    case type_rational:
	av = NewRationalDouble (&av->rational);
	break;
    default:
	break;
    }
    RETURN (av);
}

Value
DoubleReduce (Value av)
{
    ENTER ();
    double  d = av->doubles.value;

    if (!finite (d))
	ferr (0);
    if (MININT <= d && d <= MAXINT && floor (d) == d)
	av = NewInt ((int) d);
    RETURN (av);
}

Bool
DoublePrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    char    fmt[20];
    char    result[1024];

    if (prec < 0)
	prec = 15;
    if (prec > 30)
	prec = 30;
    if (width >= sizeof (result))
	width = sizeof (result) - 1;
    switch (format) {
    case 'e':
    case 'f':
    case 'g':
	break;
    default:
	format = 'g';
	break;
    }
    strcpy (fmt, "%*.*");
    fmt[4] = format;
    fmt[5] = '\0';
    sprintf (result, fmt, width, prec, av->doubles.value);
    FilePuts (f, result);
    return True;
}
    
ValueType   DoubleType = {
    { 0, 0 },
    {		/* binary */
	DoublePlus,
	DoubleMinus,
	DoubleTimes,
	DoubleDivide,
	NumericDiv,
	DoubleMod,
	DoubleLess,
	DoubleEqual,
	0,
	0,
    },
    {		/* unary */
	DoubleNegate,
	DoubleFloor,
	DoubleCeil,
    },
    DoublePromote,
    DoubleReduce,
    DoublePrint,
};

Value
NewDouble (double d)
{
    ENTER ();
    Value   ret;

    if (!finite (d))
	ferr (0);
    ret = ALLOCATE (&DoubleType.data, sizeof (Double));
    ret->value.tag = type_double;
    ret->doubles.value = d;
    RETURN (ret);
}

static double
natural_to_double (Natural *n)
{
    int	    i;
    double  result;
    digit   *d;

    result = 0.0;
    d = NaturalDigits(n) + NaturalLength(n);
    if (NaturalLess (max_double_natural, n)) {
	RaiseError ("floating overflow converting natural to double");
	return 1.0;
    }
    for (i = 0; i < NaturalLength(n); i++)
	result = result * (double) BASE + ((double) *--d);
    return result;
}

Value
NewIntegerDouble (Integer *i)
{
    ENTER ();
    double  d;
    
    d = natural_to_double (i->mag);
    if (i->sign == Negative)
	d = -d;
    RETURN (NewDouble (d));
}

Value
NewRationalDouble (Rational *r)
{
    ENTER ();
    double	result;
    Natural	*n, *d, *rem;

    n = r->num;
    d = r->den;
    result = 0.0;
    if (NaturalLess (n, d)) {
	if (NaturalLess (max_double_natural, d)) {
	    n = NaturalDivide (NaturalTimes (n, max_double_natural), d, &rem);
	    d = max_double_natural;
	}
    } else if (NaturalLess (d, n)) {
	if (NaturalLess (max_double_natural, n)) {
	    d = NaturalDivide (NaturalTimes (d, max_double_natural), n, &rem);
	    n = max_double_natural;
	}
    } else {
	result = 1.0;
    }
    if (result == 0.0)
	result = natural_to_double (n) / natural_to_double (d);
    if (r->sign != Positive)
	result = -result;
    RETURN (NewDouble (result));
}
