/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * rational.c
 *
 * operationalns on rationals
 */

#include	"nickle.h"
#include	<math.h>

int
RationalInit (void)
{
    return 1;
}
	
#if 0
static Value
natural_to_rational (Natural *n)
{
    ENTER ();
    RETURN (NewRational (Positive, n, one_natural));
}
#endif

static Value
RationalPlusHelper (Sign sign, Rational *a, Rational *b)
{
    ENTER ();
    RETURN (NewRational (sign, 
			  NaturalPlus (NaturalTimes (a->num, b->den),
				       NaturalTimes (b->num, a->den)),
			  NaturalTimes (a->den, b->den)));
}

static Value
RationalMinusHelper (Rational *a, Rational *b)
{
    ENTER ();
    Natural	*ra, *rb, *t;
    Sign	sign = Positive;

    ra = NaturalTimes (a->num, b->den);
    rb = NaturalTimes (b->num, a->den);
    if (NaturalLess (ra, rb)) 
    {
	sign = Negative;
	t = ra;
	ra = rb;
	rb = t;
    }
    RETURN (NewRational (sign, NaturalMinus (ra, rb),
			  NaturalTimes (a->den, b->den)));
}

static Value
RationalPlus (Value av, Value bv, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational, *b = &bv->rational;
    Value	ret;

    switch (catagorize_signs(a->sign, b->sign)) {
    case BothPositive:
    case BothNegative:
	ret = RationalPlusHelper (a->sign, a, b);
	break;
    case FirstPositive:
	ret = RationalMinusHelper (a, b);
	break;
    case SecondPositive:
	ret = RationalMinusHelper (b, a);
	break;
    default:
	abort();
    }
    RETURN (ret);
}

static Value
RationalMinus (Value av, Value bv, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational, *b = &bv->rational;
    Value	ret;

    switch (catagorize_signs(a->sign, b->sign)) {
    case BothPositive:
	ret = RationalMinusHelper (a, b);
	break;
    case FirstPositive:
    case SecondPositive:
	ret = RationalPlusHelper (a->sign, a, b);
	break;
    case BothNegative:
	ret = RationalMinusHelper (b, a);
	break;
    default:
	abort();
    }
    RETURN (ret);
}

static Value
RationalTimes (Value av, Value bv, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational, *b = &bv->rational;
    Sign	sign;

    sign = Positive;
    if (a->sign != b->sign)
	sign = Negative;
    RETURN (NewRational (sign, 
			 NaturalTimes (a->num, b->num),
			 NaturalTimes (a->den, b->den)));
}

static Value
RationalDivide (Value av, Value bv, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational, *b = &bv->rational;
    Sign	sign;

    if (NaturalZero (b->num))
    {
	RaiseStandardException (exception_divide_by_zero, 2,
				av, bv);
	RETURN (Void);
    }
    sign = Positive;
    if (a->sign != b->sign)
	sign = Negative;
    RETURN (NewRational (sign, 
			  NaturalTimes (a->num, b->den),
			  NaturalTimes (a->den, b->num)));
}

/*
 * Modulus for rational values.
 *
 * Sorta like for integers:
 *
 *  c/d * (a/b | c/d) + a/b % c/d = a/b
 *
 *  0 <= a/b % c/d < abs (c/d)
 *  a/b | c/d is an integer
 *
 * To calculate modulus (e/f):
 *
 *  c/d * n + e/f = a/b
 *  e/f = a/b - c/d * n
 *  (e * b * d) / f = a * d - c * b * n
 *
 *   therefore (e * b * d) / f is integer
 *
 *  c * b * n + (e * b * d) / f = a * d
 *  (e * b * d) / f = (a * d) % (c * b)
 *  e / f = ((a * d) % (c * b)) / (b * d)
 */
    
static Value
RationalMod (Value av, Value bv, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational, *b = &bv->rational;
    Natural	*rem, *div;

    if (NaturalZero (b->num))
    {
	RaiseStandardException (exception_divide_by_zero, 2,
				av, bv);
	RETURN (Void);
    }
    div = NaturalTimes (b->num, a->den);
    (void) NaturalDivide (NaturalTimes (a->num, b->den), div, &rem);
    if (a->sign == Negative && !NaturalZero (rem))
	rem = NaturalMinus (div, rem);
    RETURN (NewRational (Positive, rem, NaturalTimes (a->den, b->den)));
}


static Value
RationalLess (Value av, Value bv, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational, *b = &bv->rational;
    Rational	*t;
    int		ret;

    switch (catagorize_signs (a->sign, b->sign)) {
    case BothNegative:
	t = a;
	a = b;
	b = t;
    case BothPositive:
	if (!NaturalEqual (a->den, b->den))
	    ret = NaturalLess (NaturalTimes (a->num, b->den),
			       NaturalTimes (b->num, a->den));
	else
	    ret = NaturalLess (a->num, b->num);
	break;
    case FirstPositive:
	ret = 0;
	break;
    case SecondPositive:
	ret = 1;
	break;
    default:
	abort();
    }
    RETURN (ret ? TrueVal : FalseVal);
}

static Value
RationalEqual (Value av, Value bv, int expandOk)
{
    Rational	*a = &av->rational, *b = &bv->rational;
    
    if (a->sign == b->sign && 
	NaturalEqual (a->num, b->num) && 
	NaturalEqual (a->den, b->den))
    {
	return TrueVal;
    }
    return FalseVal;
}

static Value
RationalNegate (Value av, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational;

    RETURN (NewRational (SignNegate (a->sign), a->num, a->den));
}

static Value
RationalFloor (Value av, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational;
    Natural	*quo, *rem;

    quo = NaturalDivide (a->num, a->den, &rem);
    if (!NaturalZero (rem) && a->sign == Negative)
	quo = NaturalPlus (quo, one_natural);
    RETURN (NewInteger (a->sign, quo));
}

static Value
RationalCeil (Value av, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational;
    Natural	*quo, *rem;

    quo = NaturalDivide (a->num, a->den, &rem);
    if (!NaturalZero (rem) && a->sign == Positive)
	quo = NaturalPlus (quo, one_natural);
    RETURN (NewInteger (a->sign, quo));
}

static Value
RationalPromote (Value av, Value bv)
{
    ENTER ();

    switch (ValueTag(av)) {
    case rep_int:
	av = NewIntRational (ValueInt(av));
	break;
    case rep_integer:
	av = NewIntegerRational (&av->integer);
	break;
    default:
	break;
    }
    RETURN (av);
}
	    
static Value
RationalReduce (Value av)
{
    ENTER ();
    Rational	*a = &av->rational;

    if (NaturalEqual (a->den, one_natural))
	av = Reduce (NewInteger (a->sign, a->num));
    RETURN (av);
}

static HashValue
RationalHash (Value av)
{
    Rational	*a = &av->rational;

    return NaturalHash (a->den) ^ NaturalHash(a->num) ^ a->sign;
}

extern ValueRep    IntegerRep;

extern Natural	*NaturalFactor (Natural *, Natural *);
extern Natural	*NaturalSqrt (Natural *);
extern Natural	*NaturalIntPow (Natural *, int);
extern Natural	*NaturalPow (Natural *, Natural *);
extern Natural	*NaturalPowMod (Natural *, Natural *, Natural *);
extern Natural	*two_natural;

static Natural *
NaturalPsi(Natural *a, Natural *max)
{
    ENTER ();
    Natural *p;
    int	    n;
    Natural *ret;
    Natural *rem;
    Natural *next;
    Natural *pow;
    Natural *fact;

    ret = one_natural;
    while (!NaturalEqual (a, one_natural))
    {
	p = NaturalFactor (a, max);
	if (!p)
	{
	    ret = 0;
	    break;
	}
	n = 0;
	for (;;)
	{
	    next = NaturalDivide (a, p, &rem);
	    if (!NaturalZero (rem))
		break;
	    a = next;
	    n++;
	}
	pow = NaturalIntPow (p, n-1);
	fact = NaturalMinus (NaturalTimes (pow, p), pow);
	ret = NaturalTimes (ret, fact);
	if (max && NaturalLess (max, fact))
	    break;
    }
    RETURN (ret);
}

#if 0
static int
IntSqrt (int a)
{
    int	    l, h, m;

    l = 2;
    h = a/2;
    while ((h-l) > 1)
    {
	m = (h+l) >> 1;
	if (m * m < a)
	    l = m;
	else
	    h = m;
    }
    return h;
}

static int
IntFactor (int a)
{
    int	    v, lim;
    
    if (!a)
	return 0;
    if ((a & 1) == 0)
	return 2;
    lim = IntSqrt (a);
    for (v = 3; v <= lim; v += 2)
    {
	if (a % v == 0)
	    return v;
    }
    return a;
}

static int
IntPow (int a, int p)
{
    int	result;

    result = 1;
    while (p)
    {
	if (p & 1)
	    result = result * a;
	p >>= 1;
	if (p)
	    a = a * a;
    }
    return result;
}

static int
IntPowMod (int a, int p, int m)
{
    int	result;

    if (m >= 32767)
    {
#if DIGITBITS == 32
	signed_digit   la = a, lm = m, lr;
	lr = 1;
	while (p)
	{
	    if (p & 1)
		lr = (lr * la) % lm;
	    p >>= 1;
	    if (p)
		la = (la * la) % lm;
	}
	result = (int) lr;
#else	
	ENTER ();
	result = NaturalToInt (NaturalPowMod (NewNatural (a), 
					      NewNatural (p),
					      NewNatural (m)));
	EXIT ();
#endif
    }
    else
    {
	result = 1;
	while (p)
	{
	    if (p & 1)
		result = (result * a) % m;
	    p >>= 1;
	    if (p)
		a = (a * a) % m;
	}
    }
    return result;
}

static int
IntPsi (int a)
{
    int	    p;
    int	    n;
    int	    ret;

    ret = 1;
    while (a != 1)
    {
	p = IntFactor (a);
	n = 0;
	do
	{
	    n++;
	    a /= p;
	} while (a % p == 0);
	ret = ret * (IntPow (p, n-1) * (p - 1));
    }
    return ret;
}
#endif

typedef struct _partial {
    DataType	    *data;
    struct _partial *down;
    Natural	    *partial;
    int		    power;
} Partial, *PartialPtr;

static void PartialMark (void *object)
{
    PartialPtr	p = object;

    MemReference (p->partial);
    MemReference (p->down);
}

DataType PartialType = { PartialMark, 0, "PartialType" };

static PartialPtr
NewPartial (Natural *partial)
{
    ENTER ();
    PartialPtr	p;

    if (!partial)
	RETURN (0);
    p = ALLOCATE (&PartialType, sizeof (Partial));
    p->down = 0;
    p->partial = partial;
    p->power = 0;
    RETURN (p);
}

typedef struct _factor {
    DataType	    *data;
    struct _factor  *next;
    Natural	    *prime;
    int		    power;
    PartialPtr	    partials;
} Factor, *FactorPtr;

static void FactorMark (void *object)
{
    FactorPtr	f = object;

    MemReference (f->prime);
    MemReference (f->next);
    MemReference (f->partials);
}

DataType    FactorType = { FactorMark, 0, "FactorType" };

static FactorPtr
NewFactor (Natural *prime, int power, FactorPtr next)
{
    ENTER ();
    FactorPtr	f;

    f = ALLOCATE (&FactorType, sizeof (Factor));
    f->next = next;
    f->prime = prime;
    f->power = power;
    f->partials = 0;
    f->partials = NewPartial (prime);
    f->partials->power = 0;
    RETURN (f);
}

static FactorPtr 
GenerateFactors (Natural *n, Natural *max)
{
    ENTER ();
    FactorPtr	f = 0;
    Natural	*p;
    Natural	*largest;
    Natural	*d, *rem;

    p = 0;
    largest = NaturalSqrt (n);
    while (!NaturalEqual (n, one_natural))
    {
        int power = 1;
	for (;;)
	{
	    if (!p)
		p = two_natural;
	    else if (NaturalEqual (p, two_natural))
		p = NewNatural (3);
	    else
		p = NaturalPlus (p, two_natural);
	    
	    d = NaturalDivide (n, p, &rem);
	    if (NaturalZero (rem))
		break;
	    if (max && NaturalLess (max, p))
		RETURN(f);
	    if (NaturalLess (largest, p))
		RETURN (NewFactor (n, 1, f));
	}
	n = d;
	for (;;)
	{
	    d = NaturalDivide (n, p, &rem);
	    if (!NaturalZero (rem))
		break;
	    n = d;
	    power++;
	}
	f = NewFactor (p, power, f);
	largest = NaturalSqrt (n);
    }
    RETURN (f);
}

static Natural *
FactorBump (FactorPtr	f)
{
    PartialPtr	p, minp;
    Natural	*factor;
    
    ENTER ();
    if (!f)
	RETURN(0);
    p = f->partials;
    if (!p)
	RETURN(0);
    minp = p;
    while (p->power)
    {
	if (!p->down)
	    p->down = NewPartial (FactorBump (f->next));
	p = p->down;
	if (!p)
	    break;
	if (NaturalLess (p->partial, minp->partial))
	    minp = p;
    }
    if (!minp)
	RETURN(0);
    factor = minp->partial;
    if (minp->power < f->power)
    {
	minp->partial = NaturalTimes (minp->partial, f->prime);
	minp->power++;
    }
    else
    {
	f->partials = minp->down;
    }
    RETURN (factor);
}

static int
RationalRepeatLength (int prec, Natural *nden, int ibase)
{
    ENTER ();
    Natural	*nbase;
    Natural	*ndigits;
    FactorPtr	factors;
    Natural	*factor;
    int		digits;
    Natural	*max = 0;

    if (NaturalEqual (nden, one_natural))
	return 0;
    if (prec > 0)
	max = NewNatural (prec);
    nbase = NewNatural (ibase);
    ndigits = NaturalPsi (nden, max);
    if (!ndigits)
    {
	factor = one_natural;
	for (factor = one_natural;;
	     factor = NaturalPlus (factor, one_natural))
	{
	    if (NaturalEqual (NaturalPowMod (nbase, factor, nden),
			      one_natural))
		break;
	    if (aborting)
		break;
	    if (NaturalLess (max, factor))
	    {
		EXIT ();
		return -1;
	    }
	}
    }
    else
    {
	factors = GenerateFactors (ndigits, max);
	if (aborting)
	    return 0;
	factor = one_natural;
	while (factor)
	{
	    if (NaturalEqual (NaturalPowMod (nbase, factor, nden),
			      one_natural))
		break;
	    if (aborting)
		break;
	    factor = FactorBump (factors);
	    if (max && factor && NaturalLess (max, factor))
	    {
		EXIT ();
		return -1;
	    }
	}
    }
    if (!factor)
        factor = ndigits;
    if (NaturalLess (max_int_natural, factor))
    	factor = max_int_natural;
    digits = NaturalToInt (factor);
    EXIT ();
    return digits;
}

static void
CheckDecimalLength (int prec, Natural *nden, int ibase, int *initial, int *repeat)
{
    ENTER ();
    Natural *rem;
    Natural *nbase;
    Natural *g;
    int	    offset;
    int	    rep;

    nbase = NewNatural (ibase);
    offset = 0;
    while (!NaturalEqual ((g = NaturalGcd (nden, nbase)), one_natural))
    {
	if (aborting)
	{
	    EXIT ();
	    return;
	}
	offset++;
	if (prec >= 0 && offset > prec)
	    break;
	nden = NaturalDivide (nden, g, &rem);
    }
    if (prec >= 0 && offset >= prec)
    {
	if (offset > prec)
	    offset = -prec;
	else
	    offset = prec;
	rep = 0;
    }
    else if (NaturalEqual (nden, one_natural))
    {
	rep = 0;
    }
    else
    {
	if (prec >= 0)
	    prec -= offset;
	rep = RationalRepeatLength (prec, nden, ibase);
    }
    *initial = offset;
    *repeat = rep;
    EXIT ();
}

static Bool
RationalDecimalPrint (Value f, Value rv, char format, int base, int width, int prec, int fill)
{
    ENTER ();
    Rational	*r = &rv->rational;
    Natural	*quo;
    Natural	*partial;
    Natural	*rep, *init;
    Natural	*dig;
    int		exponent = 0;
    int		exponent_width = 0;
    char	*initial = 0, *in;
    char	*repeat = 0, *re;
    char	*whole;
    int		initial_width, repeat_width = 0;
    int		frac_width;
    int		rep_width, brace_width = 0, dot_width = 0;
    int		whole_width;
    int		fraction_width;
    int		print_width;
    int		min_prec;
    Bool	use_braces = True;

    min_prec = 0;
    if (format == 'f' || format == 'e' || format == 'g')
    {
	min_prec = prec;
	use_braces = False;
    }
    if (prec == DEFAULT_OUTPUT_PRECISION)
    {
	min_prec = 0;
	prec = 15;
    }
    else if (prec == INFINITE_OUTPUT_PRECISION)
	prec = -1;
    dig = NewNatural (base);
    /*
     * Check for small numbers for 'e' format
     */
    if (NaturalLess (r->num, r->den) && !NaturalZero(r->num))
    {
	Natural	*quo, *rem;
	Natural	*mag;
	int	bits;

	if (format == 'e' || (format == 'g' && prec > 0))
	{
	    quo = NaturalDivide (r->den, r->num, &rem);
	    bits = NaturalWidth (quo);
	    exponent = (int) ((double) bits / (log ((double) base) / log (2.0)));
	    if (exponent < 0)
		exponent = 0;
	    mag = NaturalIntPow (dig, exponent);
	    while (NaturalLess (mag, quo))
	    {
		mag = NaturalTimes (mag, dig);
		exponent++;
	    }
	    if (format == 'g' && prec > 0)
		if (prec - exponent < 3)
		    format = 'e';
	    if (format == 'e')
	    {
		int	    ev;
		rv = RationalTimes (rv, NewRational (Positive, mag, one_natural), True);
		r = &rv->rational;
		exponent_width = 3;
		ev = exponent;
		while (ev >= base)
		{
		    exponent_width++;
		    ev /= base;
		}
		exponent = -exponent;
	    }
	    else
		exponent = 0;
	}
	else
	    exponent = 0;
    }
    CheckDecimalLength (prec, r->den, base, &initial_width, &repeat_width);
    if (aborting)
    {
	EXIT ();
	return False;
    }
    if ((rep_width = repeat_width))
    {
	/*
	 * When using %f format, just fill the
	 * result with digits
	 */
	if (!use_braces && prec != -1)
	{
	    initial_width = -prec;
	    repeat_width = 0;
	    rep_width = 0;
	}
	else
	{
	    if (repeat_width < 0)
		rep_width = prec - initial_width;
	}
    }
    if (initial_width)
    {
	Natural	*half_digit;
	if (initial_width < 0)
	{
	    initial_width = -initial_width;
	    half_digit = NaturalTimes (NaturalIntPow (dig, initial_width),
				       two_natural);
	    rv = RationalPlusHelper (r->sign,
				     r,
				     &NewRational (Positive,
						   one_natural,
						   half_digit)->rational);
	    r = &rv->rational;
	}
	else
	{
	    if (!repeat_width && initial_width < min_prec)
		initial_width = min_prec;
	}
	initial = malloc (initial_width + 1);
	if (!initial)
	{
	    EXIT ();
	    return False;
	}
    }
    quo = NaturalDivide (r->num, r->den, &partial);
    whole = NaturalSprint (0, quo, base, &whole_width);
    brace_width = 0;
    if (repeat_width)
    {
	brace_width++;
	if (repeat_width > 0)
	    brace_width++;
    }
    dot_width = 0;
    if (initial_width + rep_width)
	dot_width = 1;
    /*
     * Compute how much space is available for the fractional part
     */
    if (width)
    {
	if (width < 0)
	    fraction_width = -width;
	else
	    fraction_width = width;
	fraction_width = fraction_width - (whole_width + exponent_width);
	if (fraction_width < 0)
	    fraction_width = 0;
	if (prec > 0 && fraction_width > prec + dot_width)
	    fraction_width = prec + dot_width;
    }
    else if (prec > 0)
	fraction_width = prec + dot_width;
    else
	fraction_width = -1;
    /*
     * Start paring down parts of the output to fit the desired size
     */
    while (fraction_width >= 0 &&
	   (frac_width = dot_width + initial_width + rep_width + brace_width)
	   && frac_width > fraction_width)
    {
	if (rep_width)
	{
	    if (brace_width > 1)
	    {
		brace_width = 1;
		repeat_width = -repeat_width;
	    }
	    rep_width = fraction_width - (dot_width + initial_width +
					  brace_width);
	    if (rep_width < 0)
	    {
		rep_width = 0;
	    }
	}
	else if (brace_width)
	    brace_width = 0;
	else if (initial_width)
	{
	    initial_width = fraction_width - dot_width;
	    if (initial_width < 0)
		initial_width = 0;
	}
	else
	    dot_width = 0;
    }
    if (initial_width)
    {
	init = NaturalDivide (NaturalTimes (partial,
					    NaturalIntPow (dig, initial_width)),
			      r->den,
			      &partial);
	if (aborting)
	{
	    free (initial);
	    EXIT ();
	    return False;
	}
	in = NaturalSprint (initial + initial_width + 1, 
			    init, base, &initial_width);
	if (!in)
	{
	    free (initial);
	    EXIT ();
	    return False;
	}
	while (in > initial)
	{
	    *--in = '0';
	    ++initial_width;
	}
    }
    if (rep_width)
    {
#define MAX_SENSIBLE	10000000
	if (rep_width > MAX_SENSIBLE)
	{
	    repeat_width = -1;
	    rep_width = MAX_SENSIBLE;
	}
	/*
	 * allocate the output buffer; keep trying until this works
	 */
	while (!(repeat = malloc (rep_width + 1)))
	{
	    repeat_width = -1;
	    rep_width >>= 1;
	}
	rep = NaturalDivide (NaturalTimes (partial, 
					   NaturalIntPow (dig, rep_width)),
			     r->den, 
			     &partial);
	if (aborting)
	{
	    free (initial);
	    free (repeat);
	    EXIT ();
	    return False;
	}
	re = NaturalSprint (repeat + rep_width + 1,
			    rep, base, &rep_width);
	if (!re)
	{
	    free (initial);
	    free (repeat);
	    EXIT ();
	    return False;
	}
	while (re > repeat)
	{
	    *--re = '0';
	    ++rep_width;
	}
	if (use_braces)
	{
	    rep_width++;	/* open { */
	    if (repeat_width > 0)
		rep_width++;    /* close } */
	}
    }
    fraction_width = initial_width + rep_width;
    print_width = whole_width + 1 + fraction_width + exponent_width;
    if (r->sign == Negative)
	print_width = print_width + 1;
    while (width > print_width)
    {
	FileOutchar (f, fill);
	width--;
    }
    if (r->sign == Negative)
	FileOutput (f, '-');
    FilePuts (f, whole);
    FileOutput (f, '.');
    if (initial_width)
    {
	FilePuts (f, initial);
	free (initial);
    }
    if (rep_width)
    {
	if (use_braces)
	    FileOutput (f, '{');
	FilePuts (f, repeat);
	free (repeat);
	if (use_braces && repeat_width > 0)
	    FileOutput (f, '}');
    }
    if (exponent)
    {
	FilePrintf (f, "e%d", exponent);
    }
    while (-width > print_width)
    {
	FileOutchar (f, fill);
	width++;
    }
    EXIT ();
    return True;
}

static Bool
RationalPrint (Value f, Value rv, char format, int base, int width, int prec, int fill)
{
    Rational	*r = &rv->rational;
    char	*num, *num_base, *den, *den_base;
    int		num_width, den_width;
    int		print_width;
    Bool	ret = True;
    
    if (base == 0)
	base = 10;
    switch (format) {
    case 'v':
	num_width = NaturalEstimateLength (r->num, base);
	num_base = malloc (num_width);
	num = NaturalSprint (num_base + num_width, r->num, base, &num_width);
	if (!num)
	{
	    free (num_base);
	    ret = False;
	    break;
	}
	den_width = NaturalEstimateLength (r->den, base);
	den_base = malloc (den_width);
	den = NaturalSprint (den_base + den_width, r->den, base, &den_width);
	if (!den)
	{
	    free (num_base);
	    free (den_base);
	    ret = False;
	    break;
	}
	print_width = 1 + num_width + 1 + den_width + 1;
	if (r->sign == Negative)
	    print_width++;
	while (width > print_width)
	{
	    FileOutchar (f, fill);
	    width--;
	}
	FileOutput (f, '(');
	if (r->sign == Negative)
	    FileOutput (f, '-');
	FilePuts (f, num);
	FileOutput (f, '/');
	FilePuts (f, den);
	FileOutput (f, ')');
	free (num_base);
	free (den_base);
	while (-width > print_width)
	{
	    FileOutchar (f, fill);
	    width++;
	}
	break;
    default:
	ret = RationalDecimalPrint (f, rv, format, base, width, prec, fill);
	break;
    }
    return ret;
}

static void
RationalMark (void *object)
{
    Rational   *rational = object;

    MemReference (rational->num);
    MemReference (rational->den);
}

ValueRep    RationalRep = { 
    { RationalMark, 0, "RationalRep" },    /* base */
    rep_rational,	    /* tag */
    {			    /* binary */
	RationalPlus,
	RationalMinus,
	RationalTimes,
	RationalDivide,
	NumericDiv,
	RationalMod,
	RationalLess,
	RationalEqual,
	0,
	0,
    },
    {			    /* unary */
	RationalNegate,
	RationalFloor,
	RationalCeil,
    },
    RationalPromote,
    RationalReduce,
    RationalPrint,
    0,
    RationalHash,
};

Value
NewRational (Sign sign, Natural *num, Natural *den)
{
    ENTER ();
    Value	ret;
    Natural	*g;
    Natural	*rem;

    if (NaturalZero (num))
	den = one_natural;
    else
    {
	if (NaturalLength(den) != 1 || NaturalDigits(den)[0] != 1)
	{
	    g = NaturalGcd (num, den);
	    if (NaturalLength (g) != 1 || NaturalDigits(g)[0] != 1) 
	    {
		num = NaturalDivide (num, g, &rem);
		den = NaturalDivide (den, g, &rem);
	    }
	}
    }
    ret = ALLOCATE (&RationalRep.data, sizeof (Rational));
    ret->rational.sign = sign;
    ret->rational.num = num;
    ret->rational.den = den;
    RETURN (ret);
}

Value
NewIntRational (int i)
{
    ENTER ();
    if (i < 0)
	RETURN (NewRational (Negative, NewNatural ((unsigned) -i), one_natural));
    else
	RETURN (NewRational (Positive, NewNatural ((unsigned) i), one_natural));
}

Value
NewIntegerRational (Integer *i)
{
    ENTER ();
    RETURN (NewRational (IntegerSign((Value) i), IntegerMag((Value) i),
			 one_natural));
}
