/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * divide.c
 *
 * perform natural number division
 */

#include	<config.h>

#include	"value.h"

#undef CHECK
#undef DEBUG

#if defined(DEBUG) || defined(CHECK)
void
pr (Natural *n)
{
	int	i;

	for (i = 0; i < NaturalLength(n); i++)
		(void) printf ("%u ", (unsigned int) NaturalDigits(n)[i]);
	(void) putchar ('\n');
}

void
prs (char *s, Natural *n)
{
	(void) printf ("%s ", s);
	pr (n);
}
#endif

static digit subtract(Natural *, Natural *, int);
static void multiply (Natural *, digit, Natural *);
#if 0
static Bool greaterequal (Natural *, Natural *, int);
#endif

#ifdef LBASE2
/*
 * Return shift amount needed to normalize d (MSB (d << shift) == 1)
 */
static int normalize (digit d)
{
    int	    n;
    digit   msb;

    n = 0;
    msb = 1 << (LBASE2 - 1);
    while (!(d & msb))
    {
	n++;
	d <<= 1;
    }
    return n;
}
#endif


/* 
 * magic divide support routines, these do
 * pretty special functions...
 */

/*
 * subtract b from a in place with offset implied zeros to the
 * right of b. Return if a carry out occured
 */

static digit
subtract (Natural *a, Natural *b, int offset)
{
    int		    index;
    digit	    carry;
    digit	    *at, *bt;
#ifdef LBASE2
    digit	    av, bv;
#else
    signed_digit    temp;
#endif
    int		    len;

    carry = 0;
    at = NaturalDigits(a) + offset;
    bt = NaturalDigits(b);
    index = a->length - offset;
    if (index > b->length)
	index = b->length;
    while (index--)
    {
#ifdef LBASE2
	av = *at;
	bv = *bt++ + carry;
	if (bv)
	{
	    carry = 0;
	    if ((*at = av - bv) > av)
		carry = 1;
	}
	at++;
#else
	temp = (signed_digit) *at - (signed_digit) *bt++ - carry;
	carry = 0;
	if (temp < 0) {
	    temp += BASE;
	    carry = 1;
	}
	*at++ = temp;
#endif
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

static void
add (Natural *a, Natural *b, int offset)
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
	av = *at;
	bv = *bt++ + carry;
	if (bv)
	{
	    carry = 0;
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

static __inline void
multiply (Natural *a, digit i, Natural *result)
{
    double_digit    q;
    digit	    carry;
    digit	    *at, *rt;
    int		    index;

    at = NaturalDigits(a);
    rt = NaturalDigits(result);
    carry = 0;
    index = a->length;
    while (index--) {
	q = (double_digit) i * (double_digit) *at++ + (double_digit) carry;
	carry = DivBase (q);
	*rt++ = ModBase (q);
    }
    result->length = a->length;
    if (carry)
    {
	*rt++ = carry;
	result->length++;
    }
}

#if 0
static __inline Bool
greaterequal (Natural *a, Natural *b, int offset)
{
    digit	*ad, *bd;
    int		index;

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
#endif

Natural *
NaturalDivide (Natural *a, Natural *b, Natural **remp)
{
    ENTER ();
    int		    quolen, remlen;
    Natural	    *quo, *rem;
    int		    index;
    double_digit    temp;
    digit	    temp2;
    Natural	    *partial;
    int		    offset;
    digit	    dividend1;	/* first digit of dividend */
    digit	    dividend2;	/* second digit of dividend */
    digit	    dividend3;	/* third digit of dividend */
    digit	    dividend4;	/* fourth digit of dividend */
    digit	    divisor1;	/* first digit of divisor */
    digit	    divisor2;	/* second digit of divisor */
    digit	    divisor3;	/* third digit of divisor */
    digit	    divisorc;	/* combination of first two digits */
    digit	    divisorc2;	/* combination of digits 2 and 3 */
    digit	    d;
    digit	    carry;
#ifdef LBASE2
    int		    normal;
#else
    digit	    scale;	/* scale factor for combined divisor */
#endif
    
    if (NaturalLess (a, b)) {
	quo = zero_natural;
	rem = a;
    } else if (oneNp (b)) {
	quo = a;
	rem = zero_natural;
#ifdef LBASE2
    } else if ((offset = NaturalPowerOfTwo (b)) >= 0) {
	quo = NaturalRsl (a, offset);
	rem = NaturalMask (a, offset);
#endif
    } else {
	quolen = a->length - b->length + 1;
	quo = AllocNatural (quolen);
	NaturalDigits(quo)[a->length - b->length] = 0;

	partial = AllocNatural (b->length + 1);

	remlen = a->length + 1;
	rem = AllocNatural (remlen);
	NaturalCopy (a, rem);
	NaturalDigits(rem)[remlen - 1] = 0;

	divisor1 = NaturalDigits(b)[b->length - 1];
	divisor2 = 0;
	divisor3 = 0;
	if (b->length > 1)
	{
	    divisor2 = NaturalDigits(b)[b->length - 2];
	    if (b->length > 2)
		divisor3 = NaturalDigits(b)[b->length - 3];
	}
	/*
	 * Compute a scale factor to use as many bits of
	 * the divisor as possible, then scale the
	 * first and second digit of the divisor by that
	 * scale factor
	 */
#ifdef LBASE2
	normal = normalize (divisor1);
	divisorc = divisor1 << normal;
	if (normal)
	    divisorc |= divisor2 >> (LBASE2 - normal);
	divisorc2 = divisor2 << normal;
	if (normal)
	    divisorc2 |= divisor3 >> (LBASE2 - normal);
#else
        scale = BASE / (divisor1 + 1);
	divisorc = scale * divisor1;
	if (divisor2)
	{
	    temp = (double_digit) scale * (double_digit) divisor2;
	    divisorc += temp / BASE;
	}
#endif
#ifdef DEBUG
	printf ("divisor 1 %u divisor2 %u divisorc %u\n",
		(unsigned int) divisor1, (unsigned int) divisor2,
		(unsigned int) divisorc);
#endif
	/* 
	 * division just like humans, estimate each digit,
	 * correct by checking the partial product,
	 * then subtract the resultant product from
	 * the dividend (which has been copied into rem)
	 */
	for (index = remlen-1;
	     rem->length > 0 && index >= b->length;
	     index--)
	{
#ifdef DEBUG
	    prs ("rem", rem);
#endif
	    /*
	     * estimate this digit
	     */
#ifdef DEBUG
	    printf ("digit1: %u digit2: %u divisorc: %u divisorc2\n",
		    (unsigned int) NaturalDigits(rem)[index],
		    (unsigned int) NaturalDigits(rem)[index-1],
		    (unsigned int) divisorc,
		    (unsigned int) divisorc2);
#endif
	    /* 
	     * Using the scale factor and scaled divisor computed
	     * above, compute an estimate for this digit
	     *
	     * This is computing
	     *
	     *	    d1 = top dividend digit
	     *	    d2 = next dividend digit
	     *	    v1 = top divisor digit
	     *	    v2 = next divisor digit
	     *
	     *	      (d1 * base + d2)	<- original temp
	     * temp = ----------------
	     *        (v1 + v2 / base)	<- divisorc is scaled from this
	     *
	     * To make this computation work in integers,
	     * the top and bottom are scaled by the largest amount
	     * possible (base / (v1 + 1)); for powers of two, shift
	     * the divisor until the MSB is one.
	     */
	    dividend1 = NaturalDigits(rem)[index];
	    dividend2 = dividend3 = dividend4 = 0;
	    if (index)
	    {
		dividend2 = NaturalDigits(rem)[index - 1];
		if (index > 1)
		{
		    dividend3 = NaturalDigits(rem)[index - 2];
		    if (index > 2)
			dividend4 = NaturalDigits(rem)[index - 3];
		}
	    }
	    temp = (double_digit) dividend1 << LBASE2;
	    temp |= (double_digit) dividend2;
	    if (normal)
	    {
		temp = (temp << normal) | (dividend3 >> (LBASE2 - normal));
	    }
#ifdef LBASE2
	    d = temp / divisorc;
#else
	    d = (temp * scale) / divisorc;
#endif
	    offset = index - b->length;
	    if (d)
	    {
		temp2 = dividend3;
		if (normal)
		    temp2 = (temp2 << normal) | (dividend4 >> (LBASE2 - normal));
		temp = temp - (double_digit) d * (double_digit) divisorc;
		while (temp < BASE &&
		       (double_digit) divisorc2 * d > temp * BASE + temp2)
		{
		    --d;
		    temp += (double_digit) divisorc;
		}
		if (d == 1)
		{
		    carry = subtract (rem, b, offset);
		}
		else
		{
		    multiply (b, d, partial);
		    carry = subtract (rem, partial, offset);
		}
		if (carry)
		{
#ifdef DEBUG
		    printf ("add back 0x%x\n",
			    (unsigned int) d);
		    prs ("a", a);
		    prs ("b", b);
		    prs ("rem", rem);
		    prs ("partial", partial);
#endif
		    add (rem, b, offset);
#ifdef DEBUG
		    prs ("rem", rem);
#endif
		    d--;
		}
	    }
#ifdef DEBUG
	    prs ("partial", partial);
	    printf ("result[%d] = %u\n", (int) offset, 
		    (unsigned int) d);
#endif
	    NaturalDigits(quo)[offset] = d;
	}
	/*
	 * clean up the rest of the digits
	 */
	for (offset = index - b->length; offset >= 0; offset --) {
#ifdef DEBUG
	    printf ("result[%d] zeroed\n", offset);
#endif
	    NaturalDigits(quo)[offset] = 0;
	}
	for (index = quo->length - 1; index >= 0; index--)
	    if (NaturalDigits(quo)[index] != 0) {
		quo->length = index + 1;
		break;
	    }
#ifdef DEBUG
	prs ("quo: ", quo);
	prs ("rem: ", rem);
#endif
    }
#ifdef CHECK
    {
	Natural	*check;

	check = NaturalPlus (NaturalTimes (quo, b), rem);
	if (!NaturalEqual (check, a) ||
	    !NaturalLess (rem, b))
	{
	    prs ("a:     ", a);
	    prs ("b:     ", b);
	    prs ("quo:   ", quo);
	    prs ("rem:   ", rem);
	    prs ("check: ", check);
	    printf ("divide failed\n");
	}
    }
#endif
    EXIT ();
    /*
     * Stack guaratees that this will work -- it saves a place
     * for pushing at least as far as the previous StackReset, or
     * a stack chunk, whichever is smaller.
     */
    REFERENCE (quo);
    REFERENCE (rem);
    *remp = rem;
    return quo;
}
