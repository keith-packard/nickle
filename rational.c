/* $Header$ */
/*
 * This program is Copyright (C) 1988 by Keith Packard.  IC is provided to
 * you without charge, and with no warranty.  You may give away copies of
 * IC, including source, provided that this notice is included in all the
 * files.
 */
/*
 * rational.c
 *
 * operationalns on rationals
 */

# include	"value.h"

int
RationalInit (void)
{
    return 1;
}
	
Value
natural_to_rational (Natural *n)
{
    ENTER ();
    RETURN (NewRational (Positive, n, one_natural));
}

Value
RationalPlusHelper (Sign sign, Rational *a, Rational *b)
{
    ENTER ();
    RETURN (NewRational (sign, 
			  NaturalPlus (NaturalTimes (a->num, b->den),
				       NaturalTimes (b->num, a->den)),
			  NaturalTimes (a->den, b->den)));
}

Value
RationalMinusHelper (Sign sign, Rational *a, Rational *b)
{
    ENTER ();
    Natural	*ra, *rb, *t;

    ra = NaturalTimes (a->num, b->den);
    rb = NaturalTimes (b->num, a->den);
    if (NaturalLess (ra, rb)) {
	sign = SignNegate (sign);
	t = ra;
	ra = rb;
	rb = t;
    }
    RETURN (NewRational (sign, NaturalMinus (ra, rb),
			  NaturalTimes (a->den, b->den)));
}

Value
RationalPlus (Value av, Value bv, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational, *b = &bv->rational;
    Value	ret;

    switch (catagorize_signs(a->sign, b->sign)) {
    case BothPositive:
    case BothNegative:
    default:
	ret = RationalPlusHelper (a->sign, a, b);
	break;
    case FirstPositive:
	ret = RationalMinusHelper (a->sign, a, b);
	break;
    case SecondPositive:
	ret = RationalMinusHelper (b->sign, b, a);
	break;
    }
    RETURN (ret);
}

Value
RationalMinus (Value av, Value bv, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational, *b = &bv->rational;
    Value	ret;

    switch (catagorize_signs(a->sign, b->sign)) {
    case BothPositive:
	ret = RationalMinusHelper (a->sign, a, b);
	break;
    case FirstPositive:
    case SecondPositive:
    default:
	ret = RationalPlusHelper (a->sign, a, b);
	break;
    case BothNegative:
	ret = RationalMinusHelper (b->sign, b, a);
	break;
    }
    RETURN (ret);
}

Value
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

Value
RationalDivide (Value av, Value bv, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational, *b = &bv->rational;
    Sign	sign;

    if (NaturalZero (b->num))
    {
	RaiseError ("Rational divide by zero %v / %v", av, bv);
	RETURN (Zero);
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
    
Value
RationalMod (Value av, Value bv, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational, *b = &bv->rational;
    Natural	*rem, *quo, *div;

    if (NaturalZero (b->num))
    {
	RaiseError ("Rational modulus by zero, %v %% %v", av, bv);
	RETURN (Zero);
    }
    div = NaturalTimes (b->num, a->den);
    quo = NaturalDivide (NaturalTimes (a->num, b->den), div, &rem);
    if (a->sign == Negative && !NaturalZero (rem))
	rem = NaturalMinus (div, rem);
    RETURN (NewRational (Positive, rem, NaturalTimes (a->den, b->den)));
}


Value
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
    default:
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
    }
    RETURN (ret ? One : Zero);
}

Value
RationalEqual (Value av, Value bv, int expandOk)
{
    Rational	*a = &av->rational, *b = &bv->rational;
    
    if (a->sign == b->sign && 
	NaturalEqual (a->num, b->num) && 
	NaturalEqual (a->den, b->den))
    {
	return One;
    }
    return Zero;
}

Value
RationalNegate (Value av, int expandOk)
{
    ENTER ();
    Rational	*a = &av->rational;

    RETURN (NewRational (SignNegate (a->sign), a->num, a->den));
}

Value
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

Value
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

Value
RationalPromote (Value av)
{
    ENTER ();

    switch (av->value.tag) {
    case type_int:
	av = NewIntRational (av->ints.value);
	break;
    case type_integer:
	av = NewIntegerRational (&av->integer);
	break;
    default:
	break;
    }
    RETURN (av);
}
	    
Value
RationalReduce (Value av)
{
    ENTER ();
    Rational	*a = &av->rational;

    if (NaturalEqual (a->den, one_natural))
	av = Reduce (NewInteger (a->sign, a->num));
    RETURN (av);
}

extern ValueType    IntegerType;

extern Natural	*NaturalFactor (Natural *, Natural *);
extern Natural	*NaturalSqrt (Natural *);
extern Natural	*NaturalPow (Natural *, int);
extern Natural	*NaturalPowMod (Natural *, Natural *, Natural *);
extern Natural	*two_natural;

Natural *
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
	pow = NaturalPow (p, n-1);
	fact = NaturalMinus (NaturalTimes (pow, p), pow);
	ret = NaturalTimes (ret, fact);
	if (max && NaturalLess (max, fact))
	    break;
    }
    RETURN (ret);
}

int
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

int
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

int
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

int
IntPowMod (int a, int p, int m)
{
    int	result;

    if (m >= 32767)
    {
#ifdef __GNUC__
	long long   la = a, lm = m, lr;
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

int
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

DataType PartialType = { PartialMark, 0 };

PartialPtr
NewPartial (Natural *partial)
{
    ENTER ();
    PartialPtr	p;

    if (!partial)
	RETURN (0);
    p = ALLOCATE (&PartialType, sizeof (Partial));
    p->partial = partial;
    p->power = 0;
    p->down = 0;
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

DataType    FactorType = { FactorMark, 0 };

FactorPtr
NewFactor (Natural *prime, int power, FactorPtr next)
{
    ENTER ();
    FactorPtr	f;

    f = ALLOCATE (&FactorType, sizeof (Factor));
    f->next = next;
    f->prime = prime;
    f->power = power;
    f->partials = NewPartial (prime);
    f->partials->power = 0;
    RETURN (f);
}

Natural *
NaturalMod (Natural *n, Natural *m)
{
    ENTER ();
    Natural *rem;

    (void) NaturalDivide (n, m, &rem);
    RETURN (rem);
}

#define evenp(n)    ((NaturalDigits(n)[0] & 1) == 0)

Bool
IsPrime (Natural *n)
{
    Natural *lim;
    Natural *v;

    if (evenp (n))
	return 0;
    lim = NaturalSqrt (n);
    for (v = NewNatural (3); 
	 !NaturalLess (lim, v); 
	 v = NaturalPlus (v, two_natural))
    {
	if (NaturalZero (NaturalMod (n, v)))
	    return 0;
    }
    return 1;
}

Natural *
NextPrime (Natural *n)
{
    if (NaturalEqual (n, two_natural))
	return NewNatural (3);
    do
    {
	n = NaturalPlus (n, two_natural);
    } while (!IsPrime (n));
    return n;
}

FactorPtr
GenerateFactors (Natural *n, Natural *p, Natural *max)
{
    ENTER ();
    Natural	*rem, *d;
    int		power;
    FactorPtr	factors;

    if (NaturalEqual (n, one_natural))
	RETURN(0);
    power = 0;
    for (;;)
    {
	d = NaturalDivide (n, p, &rem);
	if (exception)
	    RETURN (0);
	if (NaturalZero (rem))
	{
	    power++;
	    n = d;
	}
	else
	{
	    if (power)
	    {
		factors = NewFactor (p, power, 
				     GenerateFactors (n, NextPrime (p), max));
		break;
	    }
	    else
	    {
		p = NextPrime (p);
		if (max && NaturalLess (max, p))
		{
		    factors = 0;
		    break;
		}
	    }
	}
    }
    RETURN (factors);
}

Natural *
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

int
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
	    if (exception)
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
	factors = GenerateFactors (ndigits, two_natural, max);
	if (exception)
	    return 0;
	factor = one_natural;
	while (factor)
	{
	    if (NaturalEqual (NaturalPowMod (nbase, factor, nden),
			      one_natural))
		break;
	    if (exception)
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

void
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
	if (exception)
	{
	    EXIT ();
	    return;
	}
	offset++;
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

Bool
RationalDecimalPrint (Value f, Value rv, char format, int base, int width, int prec, unsigned char fill)
{
    ENTER ();
    Rational	*r = &rv->rational;
    Natural	*quo;
    Natural	*partial;
    Natural	*rep, *init;
    Natural	*dig;
    char	*initial = 0, *in;
    char	*repeat = 0, *re;
    char	*whole;
    int		initial_width, repeat_width;
    int		rep_width;
    int		whole_width;
    int		fraction_width;
    int		print_width;
    int		min_prec;
    Bool	use_braces = True;

    min_prec = 0;
    if (format == 'f' || format == 'e')
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
    CheckDecimalLength (prec, r->den, base, &initial_width, &repeat_width);
    if (exception)
    {
	EXIT ();
	return False;
    }
    dig = NewNatural (base);
    if ((rep_width = repeat_width))
    {
	/*
	 * When using %f format, just fill the
	 * result with digits
	 */
	if (!use_braces)
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
	    half_digit = NaturalTimes (NaturalPow (dig, initial_width),
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
    if (initial_width)
    {
	init = NaturalDivide (NaturalTimes (partial,
					    NaturalPow (dig, initial_width)),
			      r->den,
			      &partial);
	if (exception)
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
#define MAX_SENSIBLE	1000000
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
					   NaturalPow (dig, rep_width)),
			     r->den, 
			     &partial);
	if (exception)
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
    print_width = whole_width + 1 + fraction_width;
    if (r->sign == Negative)
	print_width = print_width + 1;
    while (width > print_width)
    {
	FileOutput (f, fill);
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
    while (-width > print_width)
    {
	FileOutput (f, fill);
	width++;
    }
    EXIT ();
    return True;
}

Bool
RationalPrint (Value f, Value rv, char format, int base, int width, int prec, unsigned char fill)
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
	print_width = num_width + 1 + den_width;
	if (r->sign == Negative)
	    print_width++;
	while (width > print_width)
	{
	    FileOutput (f, fill);
	    width--;
	}
	if (r->sign == Negative)
	    FileOutput (f, '-');
	FilePuts (f, num);
	FileOutput (f, '/');
	FilePuts (f, den);
	free (num_base);
	free (den_base);
	while (-width > print_width)
	{
	    FileOutput (f, fill);
	    width++;
	}
	break;
    default:
	ret = RationalDecimalPrint (f, rv, format, base, width, prec, fill);
	break;
    }
    return ret;
}

void
RationalMark (void *object)
{
    Rational   *rational = object;

    MemReference (rational->num);
    MemReference (rational->den);
}

ValueType    RationalType = { 
    { RationalMark, 0 },
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
    RationalPrint
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
    ret = ALLOCATE (&RationalType.data, sizeof (Rational));
    ret->value.tag = type_rational;
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
    RETURN (NewRational (i->sign, i->mag, one_natural));
}

Value
NewDoubleRational (double d)
{
    ENTER ();
    double	num, den;
    Sign	s;

    s = Positive;
    if (d < 0.0) {
	s = Negative;
	d = -d;
    }
    num = d;
    den = 1;
    while (num != floor (num)) {
	den *= 10;
	num *= 10;
    }
    RETURN (NewRational (s, NewDoubleNatural (num), NewDoubleNatural (den)));
}

