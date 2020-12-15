/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * natural.c
 *
 * arithmetic for natural numbers
 */

#include	<math.h>
#include	<stdio.h>
#include	"nickle.h"

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
Natural *max_signed_digit_natural;
#ifndef LBASE10
static Natural *max_tenpow_natural;
static int	tenpow_digits;
#endif
DataCachePtr	naturalCache;

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

double_digit
NaturalToDoubleDigit(Natural *n)
{
    double_digit	i;
    digit		*d;
    int			index;

    d = data(n) + length (n);
    i = 0;
    for (index = 0; index < length(n); index++)
	i = i * BASE + (double_digit) *--d;
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

#if 0
static void
NaturalClear (Natural *n)
{
    int	i;

    for (i = 0; i < length(n); i++)
	data(n)[i] = 0;
}
#endif

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

#if 0
static int
NaturalOne (Natural *n)
{
    return length (n) == 1 && data(n)[0] == 1;
}
#endif
    
Bool
NaturalLess (Natural *a, Natural *b)
{
    int	    index;
    digit   *at, *bt;

    if (length (a) < length (b))
	return True;
    else if (length (b) < length (a))
	return False;
    else {
	at = data(a) + length(a) - 1;
	bt = data(b) + length(b) - 1;
	for (index = 0; index < length(a); index++) {
	    if (*at < *bt)
		return True;
	    else if (*bt < *at)
		return False;
	    at--; bt--;
	}
	return False;
    }
}

Bool
NaturalEqual (Natural *a, Natural *b)
{
    int	    index;
    digit   *at, *bt;

    if (length (a) == length (b)) {
	at = data(a);
	bt = data(b);
	for (index = 0; index < length(a); index++)
	    if (*at++ != *bt++)
		return False;
	return True;
    }
    return False;
}

/*
 * Primitive functions that operate on sequences
 * of digits
 */

static int
DigitsLen (digit *x, int len)
{
    x += (len - 1);
    while (len && *x == 0)
    {
	len--;
	x--;
    }
    return len;
}

static int
DigitsAdd (digit *x, int xlen, digit *y, int ylen, digit *r_orig)
{
    digit   *r = r_orig;
    int	    rlen;
    digit   carry = 0;
    digit   xv, yv, rv;
    
    while (xlen && ylen)
    {
	xlen--;
	ylen--;
	rv = xv = *x++;
	yv = *y++ + carry;
	if (yv)
	{
	    carry = 0;
	    if ((rv = xv + yv) < xv)
		carry = 1;
	}
	*r++ = rv;
    }
    while (ylen)
    {
	ylen--;
	yv = *y++ + carry;
	if (yv)
	    carry = 0;
	*r++ = yv;
    }
    while (xlen)
    {
	xlen--;
	rv = xv = *x++;
	if (carry)
	{
	    yv = carry;
	    carry = 0;
	    if ((rv = xv + yv) < xv)
		carry = 1;
	}
	*r++ = rv;
    }
    if (carry)
	*r++ = carry;
    rlen = r - r_orig;
    r--;
    while (rlen && *r == 0)
    {
	r--;
	rlen--;
    }
    return rlen;
}

static int
DigitsAddInPlace (digit *x_orig, int xlen, digit *y, int ylen, int off)
{
    digit   *x = x_orig;
    digit   carry = 0;
    digit   xv, yv;

    x += off;
    xlen -= off;
    if (xlen < 0)
    {
	x += xlen;
	while (xlen++)
	    *x++ = 0;
    }
    while (xlen && ylen)
    {
	xlen--;
	ylen--;
	yv = *y++ + carry;
	if (yv)
	{
	    carry = 0;
	    xv = *x;
	    if ((*x = xv + yv) < xv)
		carry = 1;
	}
	x++;
    }
    while (ylen)
    {
	ylen--;
	yv = *y++ + carry;
	if (yv)
	    carry = 0;
	*x++ = yv;
    }
    while (xlen && carry)
    {
	xlen--;
	xv = *x;
	yv = carry;
	carry = 0;
	if ((*x = xv + yv) < xv)
	    carry = 1;
	x++;
    }
    if (carry)
	*x++ = carry;
    return xlen + (x - x_orig);
}

static int
DigitsSubInPlace (digit *x_orig, int xlen, digit *y, int ylen, int off)
{
    digit   *x = x_orig;
    digit   carry = 0;
    digit   xv, yv;

    x += off;
    xlen -= off;
    while (ylen--)
    {
	xlen--;
	xv = *x;
	yv = *y++ + carry;
	if (yv)
	{
	    carry = 0;
	    if ((*x = xv - yv) > xv)
		carry = 1;
	}
	x++;
    }
    while (carry)
    {
	xlen--;
	xv = *x;
	yv = carry;
	carry = 0;
	if ((*x = xv - yv) > xv)
	    carry = 1;
	x++;
    }
    return xlen + (x - x_orig);
}

static int
DigitTimes (digit *x, int xlen, digit y, digit *result)
{
    double_digit    q;
    digit	    carry;
    int		    rlen = xlen;

    if (y == 1)
    {
	memcpy (x, result, xlen * sizeof (digit));
	return xlen;
    }
    carry = 0;
    while (xlen--) 
    {
	q = (double_digit) y * (double_digit) *x++ + (double_digit) carry;
	carry = DivBase (q);
	*result++ = ModBase (q);
    }
    if (carry)
    {
	*result++ = carry;
	rlen++;
    }
    return rlen;
}

static int
DigitsGradeSchool (digit *x_orig, int xlen, digit *y_orig, int ylen, digit *result)
{
    digit	    *x, *y, *r, *rbase, *rloop;
    double_digit    temp;
    digit	    carry;
    digit	    xd;
    int		    xindex, yindex;
    int		    rlen;

    if (xlen == 0 || ylen == 0)
	return 0;
    if (xlen == 1)
	return DigitTimes (y_orig, ylen, *x_orig, result);
    if (ylen == 1)
	return DigitTimes (x_orig, xlen, *y_orig, result);
    memset (result, 0, (xlen + ylen + 1) * sizeof (digit));
    rbase = 0;
    x = x_orig;
    xindex = xlen;
    rbase = result;
    while (xindex--)
    {
	carry = 0;
	rloop = rbase++;
	xd = *x++;
	
	y = y_orig;
	yindex = ylen;
	while (yindex--)
	{
	    temp = (double_digit) xd * (double_digit) *y++ + (double_digit) carry;
	    carry = DivBase (temp);
	    temp = ModBase (temp);
	    r = rloop++;
	    while (temp)
	    {
		temp += (double_digit) *r;
		*r++ = ModBase (temp);
		temp = DivBase (temp);
	    }
	}
	if (carry)
	{
	    r = rloop;
	    temp = carry;
	    while (temp) 
	    {
		temp += (double_digit) *r;
		*r++ = ModBase (temp);
		temp = DivBase (temp);
	    }
	}
    }
    rlen = xlen + ylen + 1;
    r = result + (rlen - 1);
    while (rlen && *r == 0)
    {
	rlen--;
	r--;
    }
    return rlen;
}

#define KARATSUBA_LIMIT	100

/*
 * Karatsuba multiplication as found in
 @article{ karatsuba62multiplication,
     author = "A. Karatsuba and Yu Ofman",
     title = "Multiplication of multidigit numbers on automata",
     journal = "Doklady Akademii Nauk SSSR",
     volume = "145",
     number = "2",
     pages = "293--294",
     year = "1962"
 }
 */
static int
DigitsKaratsuba (digit *x, int xlen, digit *y, int ylen, digit *result, digit *tmp)
{
    /*
     * x * y = (x1 * b + x0) * (y1 * b + y0);
     *       = b^2 x1 y1 + b (x1 y0 + x0 y1) + x0 y0
     *	     = b^2 x1 y1 + b (x1 y0 + x0 y1 + x1 y1 + x0 y0) + x0 y0 - b x1 y1 - b x0 y0
     *	     = (b^2 - b) x1 y1 + b (x1 + x0) (y0 + y1) + (1 - b) x0 y0
     */
    int	    off;
    int	    off2;
    digit   *x1, *x0, *y1, *y0;
    digit   *f, *m1, *m2;
    digit   *next_tmp;
    int	    x1len, x0len, y1len, y0len;
    int	    flen, m1len, m2len;
    int	    rlen;
    
    if (aborting)
	return 0;

    if (xlen < KARATSUBA_LIMIT || ylen < KARATSUBA_LIMIT)
	return DigitsGradeSchool (x, xlen, y, ylen, result);

    off = xlen > ylen ? (xlen >> 1) : (ylen >> 1);
    off2 = off << 1;
    /*
     * Normalize partial quotients
     */
    x0 = x;
    x0len = xlen;
    if (x0len > off)
	x0len = DigitsLen (x0, off);
    if (off < xlen)
    {
	x1 = x + off;
	x1len = DigitsLen (x1, xlen - off);
    }
    else
    {
	x1 = x0;
	x1len = 0;
    }
    
    y0 = y;
    y0len = ylen;
    if (y0len > off)
	y0len = DigitsLen (y0, off);
    if (off < ylen)
    {
	y1 = y + off;
	y1len = DigitsLen (y1, ylen - off);
    }
    else
    {
	y1 = y0;
	y1len = 0;
    }
    
    /*
     * Allocate temp space
     */
    m1 = tmp;
    m2 = m1 + off + 1;
    f = tmp;		    /* overlay first factor on minuends */
    next_tmp = m2 + off + 1;
     
    /*
     * Generate middle factor first
     */
    m1len = DigitsAdd (x0, x0len, x1, x1len, m1);
    m2len = DigitsAdd (y0, y0len, y1, y1len, m2);
    
    /*
     * Compute middle factor
     */
    rlen = 0;
    if (m1len && m2len)
    {
	memset (result, 0, off * sizeof (digit));
	rlen = DigitsKaratsuba (m1, m1len, m2, m2len, result + off, next_tmp) + off;
	if (aborting)
	    return rlen;
    }
    
    /*
     * Compute first factor
     */
    if (x1len && y1len)
    {
	flen = DigitsKaratsuba (x1, x1len, y1, y1len, f, next_tmp);
	if (aborting)
	    return rlen;
	rlen = DigitsAddInPlace (result, rlen, f, flen, off2);
	rlen = DigitsSubInPlace (result, rlen, f, flen, off);
    }
    
    /*
     * Compute third factor
     */
    
    if (x0len && y0len)
    {
	flen = DigitsKaratsuba (x0, x0len, y0, y0len, f, next_tmp);
	if (aborting)
	    return rlen;
	rlen = DigitsAddInPlace (result, rlen, f, flen, 0);
	rlen = DigitsSubInPlace (result, rlen, f, flen, off);
    }
    return rlen;
}

Natural *
NaturalPlus (Natural *a, Natural *b)
{
    ENTER ();
    Natural	    *result;

    result = AllocNatural (max(length(a), length(b)) + 1);
    result->length = DigitsAdd (data(a), length(a), 
				data(b), length(b),
				data(result));
    RETURN (result);
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
    Natural *result;
    int	    rlen;
    digit   *tmp;
    int	    tmp_len;

    if (length (a) < KARATSUBA_LIMIT || length (b) < KARATSUBA_LIMIT)
    {
	if (zeroNp (a) || zeroNp (b))
	    RETURN (zero_natural);
	if (oneNp (a))
	    RETURN(b);
	if (oneNp (b))
	    RETURN (a);
	result = AllocNatural (length(a) + length (b) + 1);
	result->length = DigitsGradeSchool (data(a), length(a), data(b), length (b), data(result));
    }
    else
    {
	if (length (a) > length (b))
	    rlen = length (a) << 1;
	else
	    rlen = length (b) << 1;
	result = AllocNatural (rlen);
	tmp_len = rlen << 3;
	tmp = AllocateTemp (tmp_len * sizeof (digit));
	rlen = DigitsKaratsuba (data(a), length (a), data(b), length (b), data(result), tmp);
	if (aborting)
	    RETURN(zero_natural);
	tmp = data(result) + (rlen - 1);
	while (rlen && *tmp == 0)
	{
	    rlen--;
	    tmp--;
	}
	result->length = rlen;
    }
    
    RETURN (result);
}

Natural *
NaturalLand (Natural *a, Natural *b)
{
    ENTER ();
    digit	*at, *bt, *rt;
    Natural	*result;
    int		resultlen;

    resultlen = length (a);
    if (resultlen > length(b))
	resultlen = length(b);
    at = data(a) + (resultlen-1);
    bt = data(b) + (resultlen-1);
    while (resultlen > 0 && (*at & *bt) == 0)
    {
	resultlen--;
	at--;
	bt--;
    }
    if (resultlen == 0)
	RETURN (zero_natural);
    result = AllocNatural (resultlen);
    rt = data(result) + (resultlen-1);
    while (resultlen-- > 0)
	*rt-- = *at-- & *bt--;
    RETURN (result);
}

Natural *
NaturalLor (Natural *a, Natural *b)
{
    ENTER ();
    digit	*at, *bt, *rt;
    Natural	*result;
    int		alength;
    int		blength;

    alength = length(a);
    blength = length(b);
    if (alength < blength)
    {
	result = a;
	a = b;
	b = result;
	alength = length(a);
	blength = length(b);
    }
    if (alength == 0)
	RETURN (zero_natural);
    result = AllocNatural (alength);
    at = data(a);
    bt = data(b);
    rt = data(result);
    alength -= blength;
    while (blength--)
	*rt++ = *at++ | *bt++;
    while (alength--)
	*rt++ = *at++;
    RETURN (result);
}

Natural *
NaturalCompliment (Natural *a, int len)
{
    ENTER ();
    digit   *at, *rt;
    Natural *result;
    int	    resultlen;

    resultlen = length (a);
    at = data(a) + (resultlen-1);
    while (resultlen > len && ~*at == 0)
    {
	resultlen--;
	at--;
    }
    if (resultlen == 0)
	RETURN (zero_natural);
    if (resultlen > len)
	len = resultlen;
    result = AllocNatural (len);
    rt = data(result) + (len-1);
    while (len > resultlen)
    {
	*rt-- = ~0;
	len--;
    }
    while (resultlen-- > 0)
	*rt-- = ~*at--;
    RETURN (result);
}

Natural *
NaturalNegate (Natural *n, int len)
{
    ENTER ();
    RETURN (NaturalPlus (NaturalCompliment (n, len), one_natural));
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
	if (aborting)
	    break;
	if (max && NaturalLess (max, v))
	    RETURN (0);
    }
    RETURN (n);
}

Natural *
NaturalIntPow (Natural *n, int p)
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
	if (aborting)
	    break;
    }
    RETURN (result);
}

Natural *
NaturalPow (Natural *n, Natural *p)
{
    ENTER ();
    Natural *result;

    result = one_natural;
    while (!zerop (p))
    {
	if (data(p)[0] & 1)
	    result = NaturalTimes (result, n);
	p = NaturalRsl(p, 1);
	if (!zerop (p))
	    n = NaturalTimes (n, n);
	if (aborting)
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
	if (aborting)
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

static char *
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
	while (digits-- > 0)
	    *--result = '0';
    return result;
}

static char *
NaturalSplit (char *result, Natural *a, Natural **divisors, int base, int digits, Bool fill)
{
    ENTER ();
    Natural *q, *r;
    Bool    rfill;

    if (aborting)
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
	    if (aborting)
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
	    if (aborting)
	    {
		r = 0;
		break;
	    }
	} while (NaturalLess (divisor, a));
	if (!aborting)
	    r = NaturalSplit (r, a, divisors + idivisor - 1, base, digits, False);
	free (divisors);
    }
    if (width && r)
	*width = (result - 1) - r;
    EXIT ();
    return r;
}

DataType NaturalType = { 0, 0, "NaturalType" };

Natural *
AllocNatural (int size)
{
    Natural *result;

    result = ALLOCATE (&NaturalType, sizeof (Natural) + size * sizeof (digit));
    result->length = size;
    return result;
}

static Natural *
NewDoubleDigitNaturalReal (double_digit dd)
{
    Natural	*result;
    int		    len;
    double_digit    temp;
    digit	    *d;

    len = 0;
    temp = dd;
    while (temp) {
	len++;
	temp = DivBase (temp);
    }
    result = AllocNatural (len);
    temp = dd;
    d = data(result);
    while (temp) {
	*d++ = ModBase (temp);
	temp = DivBase (temp);
    }
    return result;
}

#define NATURAL_CACHE_SIZE  8191

Natural *
NewDoubleDigitNatural (double_digit dd)
{

    switch (dd) {
    case 0:
	return zero_natural;
    case 1:
	return one_natural;
    case 2:
	return two_natural;
    case MAX_NICKLE_INT:
	return max_int_natural;
    default:
	{
	    digit	    l = ModBase(dd), u = DivBase(dd);
	    unsigned 	    c = l % NATURAL_CACHE_SIZE;
	    Natural	    **re = (Natural **) DataCacheValues(naturalCache) + c;
	    Natural	    *ret = *re;
	    digit	    *d;

	    if (ret)
	    {
		d = data(ret);
		if (l == d[0] && u == (ret->length == 1 ? 0 : d[1]))
		{
		    REFERENCE (ret);
		    return ret;
		}
	    }
	    ret = NewDoubleDigitNaturalReal (dd);
	    *re = ret;
	    return ret;
	}
    }
}

Natural *
NewNatural (unsigned value)
{
    return NewDoubleDigitNatural ((double_digit) value);
}

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

    if (v->length == 0)
	RETURN (zero_natural);
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
    if (length <= 0)
	RETURN (zero_natural);
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

    if (v->length == 0)
	RETURN (zero_natural);
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

void
NaturalDigitMultiply (Natural *a, digit i, Natural *result)
{
    result->length = DigitTimes (data(a), length(a), i,
				 data(result));
}

/*
 * subtract b from a in place with offset implied zeros to the
 * right of b. Return if a carry out occured
 */

digit
NaturalSubtractOffset (Natural *a, Natural *b, int offset)
{
    int		    index;
    digit	    carry;
    digit	    *at, *bt;
    digit	    av, bv;
    int		    len;

    carry = 0;
    at = NaturalDigits(a) + offset;
    bt = NaturalDigits(b);
    index = a->length - offset;
    if (index > b->length)
	index = b->length;
    while (index--)
    {
	av = *at;
	bv = *bt++ + carry;
	if (bv)
	{
	    carry = 0;
	    if ((*at = av - bv) > av)
		carry = 1;
	}
	at++;
    }
    if (carry && a->length > b->length + offset)
    {
	*at = *at - carry;
	carry = 0;
    }
    len = a->length;
    at = NaturalDigits(a) + len;
    while (len > 0 && *--at == 0)
	len--;
    a->length = len;
    return carry;
}

digit
NaturalSubtractOffsetReverse (Natural *a, Natural *b, int offset)
{
    int		    index;
    digit	    carry;
    digit	    *at, *bt;
    digit	    av, bv;
    int		    len;

    carry = 0;
    at = NaturalDigits(a) + offset;
    bt = NaturalDigits(b);
    index = a->length - offset;
    if (index > b->length)
	index = b->length;
    while (index--)
    {
	av = *at + carry;
	bv = *bt++;
	if (bv)
	{
	    carry = 0;
	    if ((*at = bv - av) > bv)
		carry = 1;
	}
	at++;
    }
    if (carry && a->length > b->length + offset)
    {
	*at = carry;
	carry = 0;
    }
    len = a->length;
    at = NaturalDigits(a) + len;
    while (len > 0 && *--at == 0)
	len--;
    a->length = len;
    return carry;
}

void
NaturalAddOffset (Natural *a, Natural *b, int offset)
{
    int	    index;
    digit   carry;
    digit   *at, *bt;
    digit   av, bv;

    carry = 0;
    at = NaturalDigits(a) + offset;
    bt = NaturalDigits(b);
    index = b->length;
    while (index--)
    {
	bv = *bt++ + carry;
	if (bv)
	{
	    carry = 0;
	    av = *at;
	    if ((*at = av + bv) < av)
		carry = 1;
	}
	at++;
    }
    if (carry)
	*at = *at + carry;
    if (at == NaturalDigits(a) + a->length - 1)
    {
	while (a->length && *at == 0)
	{
	    at--;
	    a->length--;
	}
    }
}

Bool
NaturalGreaterEqualOffset (Natural *a, Natural *b, int offset)
{
    digit       *ad, *bd;
    int         index;

    if (a->length > b->length + offset)
	return True;
    if (a->length < b->length + offset)
	return False;
    ad = NaturalDigits(a) + a->length - 1;
    bd = NaturalDigits(b) + b->length - 1;
    index = b->length;
    while (index--)
    {
	if (*ad > *bd)
	    return True;
	if (*ad < *bd)
	    return False;
	--ad;
	--bd;
    }
    return True;
}

HashValue
NaturalHash (Natural *a)
{
    return HashCrc32 ((unsigned char *) &a->length,
		      sizeof (int) + sizeof (digit) * a->length);
}

int
NaturalInit (void)
{
    ENTER ();
#ifndef LBASE10
    int		max_tenpow, i;
#endif

    naturalCache = NewDataCache (NATURAL_CACHE_SIZE);
    zero_natural = NewDoubleDigitNaturalReal (0);
    MemAddRoot (zero_natural);
    one_natural = NewDoubleDigitNaturalReal (1);
    MemAddRoot (one_natural);
    two_natural = NewDoubleDigitNaturalReal (2);
    MemAddRoot (two_natural);
    max_int_natural = NewDoubleDigitNaturalReal (MAX_NICKLE_INT);
    MemAddRoot (max_int_natural);
    max_signed_digit_natural = NewDoubleDigitNaturalReal (MAX_NICKLE_SIGNED_DIGIT);
    MemAddRoot(max_signed_digit_natural);
#ifndef LBASE10
    tenpow_digits = (int) floor (log10 ((double) MAX_NICKLE_INT));
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

