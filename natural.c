/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * natural.c
 *
 * arithmetic for natural numbers
 */

#include	<config.h>

#include	<stdio.h>
#include	<math.h>
#include	"value.h"

# define length(n)	((n)->length)
# define data(n)	NaturalDigits(n)
# define max(a,b)	((a) > (b) ? (a) : (b))
# define zerop(n)	(length(n) == 0)

#define BIGDOUBLE		((double)1.79769313486231470e+308)

#ifndef MAXDOUBLE
# define MAXDOUBLE		((double)1.79769313486231470e+308)
#endif

Natural	*zero_natural;
Natural	*one_natural;
Natural *two_natural;
Natural *max_int_natural;
Natural	*max_double_natural;
#ifndef LBASE10
static Natural *max_tenpow_natural;
static int	tenpow_digits;
#endif

int
NaturalInit (void)
{
    ENTER ();
#ifndef LBASE10
    extern double	log10(double), floor(double);
    int		max_tenpow, i;
#endif

    zero_natural = NewNatural (0);
    MemAddRoot (zero_natural);
    one_natural = NewNatural (1);
    MemAddRoot (one_natural);
    two_natural = NewNatural (2);
    MemAddRoot (two_natural);
    max_int_natural = NewNatural (MAXINT);
    MemAddRoot (max_int_natural);
    max_double_natural = NewDoubleNatural ((double) BIGDOUBLE);
    MemAddRoot (max_double_natural);
#ifndef LBASE10
    tenpow_digits = (int) floor (log10 ((double) MAXINT));
    max_tenpow = 1;
    for (i = 0; i < tenpow_digits; i++)
	max_tenpow *= 10;
#ifdef DEBUG
    printf ("max_tenpow: %d\n", max_tenpow);
#endif
    max_tenpow_natural = NewNatural (max_tenpow);
    MemAddRoot (max_tenpow_natural);
#endif	
    EXIT ();
    return 1;
}

int
NaturalToInt (Natural *n)
{
    int		i;
    digit	*d;
    int		index;

    d = data(n) + length (n);
    i = 0;
    for (index = 0; index < length(n); index++)
	i = i * BASE + (int) *--d;
    return i;
}

void
NaturalCopy (Natural *a, Natural *b)
{
    digit	*ad, *bd;
    int	index;

    length(b) = length(a);
    ad = data(a);
    bd = data(b);
    for (index = 0; index < length(a); index++)
	*bd++ = *ad++;
}

void
NaturalClear (Natural *n)
{
    int	i;

    for (i = 0; i < length(n); i++)
	data(n)[i] = 0;
}

Bool
NaturalEven (Natural *n)
{
    if (!length (n) || (data(n)[0] & 1) == 0)
	return True;
    return False;
}

Bool
NaturalZero (Natural *n)
{
    return length (n) == 0;
}

int
NaturalOne (Natural *n)
{
    return length (n) == 1 && data(n)[0] == 1;
}
    
int
NaturalLess (Natural *a, Natural *b)
{
    int	    index;
    digit   *at, *bt;

    if (length (a) < length (b))
	return 1;
    else if (length (b) < length (a))
	return 0;
    else {
	at = data(a) + length(a) - 1;
	bt = data(b) + length(b) - 1;
	for (index = 0; index < length(a); index++) {
	    if (*at < *bt)
		return 1;
	    else if (*bt < *at)
		return 0;
	    at--; bt--;
	}
	return 0;
    }
}

int
NaturalEqual (Natural *a, Natural *b)
{
    int	    index;
    digit   *at, *bt;

    if (length (a) == length (b)) {
	at = data(a);
	bt = data(b);
	for (index = 0; index < length(a); index++) {
	    if (*at < *bt)
		return 0;
	    else if (*bt < *at)
		return 0;
	    at++; bt++;
	}
	return 1;
    }
    return 0;
}

Natural *
NaturalPlus (Natural *a, Natural *b)
{
    ENTER ();
    int		    resultlen;
    Natural	    *result;
    double_digit    temp;
    int		    index;
    digit	    *at, *bt, *rt;
    digit	    carry;

    resultlen = max(length(a), length(b)) + 1;
    result = AllocNatural (resultlen);
    at = data(a);
    bt = data(b);
    rt = data(result);
    carry = 0;
    temp = 0;
    for (index = 0; index < resultlen; index++) {
	temp = (double_digit) (index < length(a) ? *at++ : 0) +
	(double_digit) (index < length(b) ? *bt++ : 0) +
	(double_digit) carry;
	carry = DivBase (temp);
	temp = ModBase (temp);
	*rt++ = temp;
    }
    /*
     * if the last digit was zero (i.e. no carry),
     * shorten the number
     */
    if (temp == 0)
	length(result) = length(result) - 1;
    RETURN(result);
}

Natural *
NaturalMinus (Natural *a, Natural *b)
{
    ENTER ();
    int		    resultlen;
    Natural	    *result;
    signed_digit    temp, carry;
    digit	    *at, *bt, *rt;
    int		    index;
    int		    len;

    resultlen = length(a);
    result = AllocNatural (resultlen);
    at = data(a);
    bt = data(b);
    rt = data(result);
    carry = 0;
    len = -1;
    for (index = 0; index < resultlen; index++) {
	temp = ((signed_digit) (index < length(a) ? *at++ : 0) -
		(signed_digit) (index < length(b) ? *bt++ : 0) -
		(signed_digit) carry);
	carry = 0;
	if (temp < 0) {
	    temp += BASE;
	    carry = 1;
	}
	if (temp > 0)
	    len = index;
	*rt++ = temp;
    }
    length(result) = len + 1;
    RETURN(result);
}

Natural *
NaturalTimes (Natural *a, Natural *b)
{
    ENTER ();
    register int	    cindex;
    register double_digit   temp;
    register digit	    carry;
    register digit	    *rt;
    register digit	    *at, *bt;
    Natural		    *result;
    int			    resultlen;
    digit		    *aend, *bend;
    int			    aindex, bindex;

    if (oneNp (a)) {
	RETURN (b);
    } else if (oneNp (b)) {
	RETURN (a);
    }
    resultlen = length(a) + length(b) + 1;
    result = AllocNatural (resultlen);
    rt = data(result);
    aindex = 0;
    aend = data(a) + length(a);
    bend = data(b) + length(b);
    for (at = data(a); at < aend; at++) {
	carry = 0;
	bindex = aindex;
	for (bt = data(b); bt < bend; bt++) {
	    temp = (double_digit) *at * (double_digit) *bt + (double_digit) carry;
	    carry = DivBase (temp);
	    temp = ModBase (temp);
	    cindex = bindex;
	    while (temp) {
		temp += rt[cindex];
		rt[cindex] = ModBase (temp);
		temp = DivBase (temp);
		cindex++;
	    }
	    bindex++;
	}
	if (carry) {
	    cindex = bindex;
	    temp = carry;
	    while (temp) {
		temp += rt[cindex];
		rt[cindex] = ModBase (temp);
		temp = DivBase (temp);
		cindex++;
	    }
	}
	aindex++;
    }
    cindex = resultlen - 1;
    while (cindex >= 0) {
	if (rt[cindex] != 0)
	    break;
	--cindex;
    }
    length(result) = cindex + 1;
    RETURN (result);
}

Natural *
NaturalSqrt (Natural *n)
{
    ENTER ();
    Natural *l, *h, *m, *rem;

    l = two_natural;
    h = NaturalDivide (n, two_natural, &rem);
    while (NaturalLess (one_natural,
			NaturalMinus (h, l)))
    {
	m = NaturalDivide (NaturalPlus (l, h), two_natural, &rem);
	if (NaturalLess (NaturalTimes (m, m), n))
	    l = m;
	else
	    h = m;
    }
    RETURN (h);
}

Natural *
NaturalFactor (Natural *n, Natural *max)
{
    ENTER ();
    Natural *v, *lim, *rem;

    if (zerop (n))
	RETURN(zero_natural);
    if ((data(n)[0] & 1) == 0)
	RETURN(two_natural);
    lim = NaturalSqrt (n);
    for (v = NewNatural (3); 
	 !NaturalLess (lim, v);
	 v = NaturalPlus (v, two_natural))
    {
	(void) NaturalDivide (n, v, &rem);
	if (zerop (rem))
	    RETURN (v);
	if (exception)
	    break;
	if (max && NaturalLess (max, v))
	    RETURN (0);
    }
    RETURN (n);
}

Natural *
NaturalPow (Natural *n, int p)
{
    ENTER ();
    Natural *result;

    result = one_natural;
    while (p)
    {
	if (p & 1)
	    result = NaturalTimes (result, n);
	p >>= 1;
	if (p)
	    n = NaturalTimes (n, n);
	if (exception)
	    break;
    }
    RETURN (result);
}

#define evenp(n)    ((zerop (n) || ((data(n)[0] & 1) == 0)))

Natural *
NaturalPowMod (Natural *n, Natural *p, Natural *m)
{
    ENTER ();
    Natural *result;
    Natural *rem;

    result = one_natural;
    while (!zerop (p))
    {
	if (!evenp (p))
	    (void) NaturalDivide (NaturalTimes (result, n), m, &result);
	p = NaturalDivide (p, two_natural, &rem);
	if (!zerop(p))
	    (void) NaturalDivide (NaturalTimes (n, n), m, &n);
	if (exception)
	    break;
    }
    RETURN (result);
}

static int
digit_width (digit d, int base)
{
    int	    width = 1;
    while (d >= base)
    {
	width++;
	d /= base;
    }
    return width;
}

int
NaturalEstimateLength (Natural *a, int base)
{
    if (length (a) == 0)
	return 2;
    return length(a) * digit_width (MAXDIGIT, base) + 1;
}
    
char	*naturalBuffer;
int	naturalBufferSize;

char *
NaturalBottom (char *result, digit partial, int base, int digits, Bool fill)
{
    digit   dig;
    
    do
    {
	dig = partial % base;
	if (dig < 10)
	    dig = '0' + dig;
	else
	    dig = 'a' + dig - 10;
	*--result = dig;
	digits--;
	partial = partial / base;
    } while (partial);
    if (fill)
	while (digits--)
	    *--result = '0';
    return result;
}

char *
NaturalSplit (char *result, Natural *a, Natural **divisors, int base, int digits, Bool fill)
{
    ENTER ();
    Natural *q, *r;
    Bool    rfill;

    if (exception)
	return 0;
    if (zerop (a))
    {
	if (fill)
	    while (digits--)
		*--result = '0';
    }
    else if (!divisors[0])
    {
	result = NaturalBottom (result, data(a)[0], base, digits, fill);
    }
    else
    {
	q = NaturalDivide (a, divisors[0], &r);
	digits = digits / 2;
	divisors--;
	rfill = True;
	if (zerop (q))
	    rfill = fill;
	result = NaturalSplit (result, r, divisors, 
			       base, digits, rfill);
	if (rfill)
	    result = NaturalSplit (result, q, divisors, 
				   base, digits, fill);
    }
    EXIT ();
    return result;
}

char *
NaturalSprint (char *result, Natural *a, int base, int *width)
{
    ENTER ();
    int		    len;
    double_digit    max_base;
    int		    digits;
    digit	    *t;
    Natural	    *divisor;
    char	    *r;
    digit	    partial;
    int		    print_width;
    Natural	    **divisors;
    int		    ndivisors;
    int		    idivisor;
    
    if (!result)
    {
	/*
	 * Allocate temporary space for the string of digits
	 */
	print_width = NaturalEstimateLength (a, base);
	if (naturalBufferSize < print_width)
	{
	    if (naturalBuffer)
		free (naturalBuffer);
	    naturalBuffer = malloc (print_width);
	    if (!naturalBuffer)
	    {
		naturalBufferSize = 0;
		EXIT ();
		return 0;
	    }
	    naturalBufferSize = print_width;
	}
	result = naturalBuffer + naturalBufferSize;
    }
    r = result;
    *--r = '\0';
    len = length (a);
    if (len == 0)
    {
	*--r = '0';
	if (width)
	    *width = 1;
	EXIT ();
	return r;
    }
    /*
     * Compute the number of base digits which can be
     * held in BASE
     */
    max_base = base;
    digits = 0;
    while (max_base <= BASE)
    {
	max_base *= base;
	digits++;
    }
    max_base /= base;
    t = 0;
    divisor = 0;
    if (max_base == BASE)
    {
	t = data(a);
	while (len)
	{
	    if (exception)
	    {
		r = 0;
		break;
	    }
	    partial = *t++;
	    len--;
	    r = NaturalBottom (r, partial, base, digits, len != 0);
	}
    }
    else
    {
	divisor = NewNatural ((unsigned) max_base);
	divisors = 0;
	ndivisors = 0;
	idivisor = 0;
	do
	{
	    if (idivisor >= ndivisors - 1)
	    {
		ndivisors += 128;
		if (divisors)
		    divisors = realloc (divisors, ndivisors * sizeof (Natural *));
		else
		    divisors = malloc (ndivisors * sizeof (Natural *));
		if (!divisors)
		    return 0;
	    }
	    if (!idivisor)
		divisors[idivisor++] = 0;
	    divisors[idivisor++] = divisor;
	    divisor = NaturalTimes (divisor, divisor);
	    digits = digits * 2;
	} while (NaturalLess (divisor, a));
	r = NaturalSplit (r, a, divisors + idivisor - 1, base, digits, False);
	free (divisors);
    }
    if (width && r)
	*width = (result - 1) - r;
    EXIT ();
    return r;
}

DataType NaturalType = { 0, 0 };

Natural *
AllocNatural (int size)
{
    ENTER();
    Natural *result;

    result = ALLOCATE (0, sizeof (Natural) + size * sizeof (digit));
    result->length = size;
    RETURN (result);
}

Natural *
NewNatural (unsigned value)
{
    ENTER ();
    Natural	    *result;
    int		    len;
    double_digit    temp;
    digit	    *d;

    len = 0;
    temp = value;
    while (temp) {
	len++;
	temp = DivBase (temp);
    }
    result = AllocNatural (len);
    temp = value;
    d = data(result);
    while (temp) {
	*d++ = ModBase (temp);
	temp = DivBase (temp);
    }
    RETURN(result);
}

static double
logBASE (double d)
{
    return log (d) / log ((double) BASE);
}

double
expBASE (int i)
{
    double	result;

    if (!i)
	return 1.0;
    result = 1.0;
    while (--i > 0)
	result *= BASE;
    return result * BASE;
}

Natural *
NewDoubleNatural (double d)
{
    ENTER ();
    Natural	    *new;
    double	    l, exp;
    int		    digits, i;
    digit	    dig;

    if (d < 1.0)
	RETURN (zero_natural);
    l = logBASE (d);
    digits = ceil (l);
    exp = expBASE (digits - 1);
    d /= exp;
    new = AllocNatural (digits);
    i = digits;
    while (i) {
	dig = ((double_digit) d) % BASE;
	--i;
	data(new)[i] = dig;
	d -= dig;
	if (i)
	    d *= (double) BASE;
    }
    if (data(new)[digits-1] == 0)
	length(new)--;
    RETURN(new);
}

#ifdef LBASE2
Natural *
NaturalRsl (Natural *v, int shift)
{
    ENTER ();
    Natural *r;
    digit   *vt, *rt;
    digit   d1, d2;
    int	    length;
    int	    dshift;
    int	    index, last;

#ifdef LLBASE2
    dshift = (shift >> LLBASE2);
    shift = (shift & (LBASE2 - 1));
#else
    dshift = shift / LBASE2;
    shift = shift % LBASE2;
#endif
    length = v->length - dshift;
    index = length;
    last = 1;
    if ((NaturalDigits(v)[v->length - 1] >> shift) == 0)
    {
	length--;
	last = 0;
    }
    r = AllocNatural (length);
    rt = NaturalDigits (r);
    vt = NaturalDigits (v) + dshift;
    if (shift)
    {
	d2 = *vt++;
	while (--index)
	{
	    d1 = d2;
	    d2 = *vt++;
	    *rt++ = (d1 >> shift) | (d2 << (LBASE2 - shift));
	}
	if (last)
	    *rt++ = (d2 >> shift);
    }
    else
    {
	while (length--)
	{
	    *rt++ = *vt++;
	}
    }
    RETURN (r);
}

Natural *
NaturalLsl (Natural *v, int shift)
{
    ENTER ();
    Natural *r;
    digit   *vt, *rt;
    digit   d1, d2;
    int	    length;
    int	    dshift;
    int	    index;
    int	    last;

#ifdef LLBASE2
    dshift = (shift >> LLBASE2);
    shift = (shift & (LBASE2 - 1));
#else
    dshift = shift / LBASE2;
    shift = shift % LBASE2;
#endif
    length = v->length + dshift;
    index = v->length;
    last = 0;
    if (shift)
    {
	if ((NaturalDigits(v)[v->length - 1] >> (LBASE2 - shift)) != 0)
	{
	    length++;
	    last = 1;
	}
    }
    r = AllocNatural (length);
    rt = NaturalDigits (r);
    while (dshift--)
	*rt++ = 0;
    vt = NaturalDigits (v);
    if (shift)
    {
	d2 = *vt++;
	*rt++ = d2 << shift;
	while (--index)
	{
	    d1 = d2;
	    d2 = *vt++;
	    *rt++ = (d1 >> (LBASE2 - shift)) | (d2 << shift);
	}
	if (last)
	    *rt++ = (d2 >> (LBASE2 - shift));
    }
    else
    {
	while (index--)
	    *rt++ = *vt++;
    }
    RETURN (r);
}

Natural *
NaturalMask (Natural *v, int bits)
{
    ENTER ();
    Natural *r;
    digit   *vt, *rt;
    digit   mask;
    int	    length;

#ifdef LLBASE2
    length = (bits + LBASE2) >> LLBASE2;
    mask = bits & (LBASE2 - 1);
#else
    length = (bits + LBASE2) / LBASE2;
    mask = bits % LBASE2;
#endif
    mask = (1 << mask) - 1;
    if (length > v->length)
    {
	length = v->length;
	mask = (digit) ~0;
    }
    while (length && (NaturalDigits(v)[length - 1] & mask) == 0)
    {
	length--;
	mask = (digit) ~0;
    }
    r = AllocNatural (length);
    rt = NaturalDigits (r);
    vt = NaturalDigits (v);
    if (length)
    {
	length--;
	while (length--)
	    *rt++ = *vt++;
	*rt = *vt & mask;
    }
    RETURN (r);
}

int
NaturalPowerOfTwo (Natural *v)
{
    int	    bit;
    int	    l;
    digit   *vt, last;
    
    if (!v->length)
	return -1;
    vt = NaturalDigits(v);
    l = v->length - 1;
    while (l--)
    {
	if (*vt++ != 0)
	    return -1;
    }
    last = *vt;
    if (last & (last - 1))
	return -1;
    bit = (v->length - 1) * LBASE2;
    while (!(last & 1))
    {
	bit++;
	last >>= 1;
    }
    return bit;
}

#endif
