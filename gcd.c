/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * gcd.c
 *
 * compute gcd of two natural numbers
 */

#include	"nickle.h"

static void
gcd_right_shift (Natural *v, int shift)
{
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
    rt = NaturalDigits (v);
    vt = NaturalDigits (v) + dshift;
    v->length = length;
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
}

/*
 * compute u/v mod 2**s
 */
digit
DigitBmod (digit u, digit v, int s)
{
    int	    i;
    digit   umask = 0;
    digit   imask = 1;
    digit   m = 0;

    for (i = 0; i < s; i++)
    {
	umask |= imask;
	if (u & umask)
	{
	    u = u - v;
	    m = m | imask;
	}
	imask <<= 1;
	v <<= 1;
    }
    return m;
}

int
NaturalWidth (Natural *u)
{
    int	    w;
    digit   top;
    
    if (NaturalZero (u))
	return 0;
    w = NaturalLength (u) - 1;
    top = NaturalDigits(u)[w];
    w <<= LLBASE2;
    while (top)
    {
	w++;
	top >>= 1;
    }
    return w;
}

int
IntWidth (int i)
{
    int	w;

    if (i < 0)
	i = -i;
    w = 0;
    while (i)
    {
	w++;
	i >>= 1;
    }
    return w;
}

#ifdef DEBUG
#define START	    int steps = 0
#define STEP	    steps++
#define FINISH(op)  FilePrintf (FileStdout, "%s %d steps\n", op, steps)
#else
#define START
#define STEP
#define FINISH(op)
#endif

Natural *
NaturalBdivmod (Natural *u_orig, Natural *v)
{
    ENTER ();
    Natural *u;
    digit   b, v0;
    Natural *t;
    int	    i;
    int	    d;
    digit   dmask;
    START;
    
    u = AllocNatural (u_orig->length);
    NaturalCopy (u_orig, u);
    t = AllocNatural (v->length + 2);
    d = NaturalWidth (u) - NaturalWidth (v) + 1;
    v0 = NaturalDigits(v)[0];
    i = 0;
    while (d > DIGITBITS)
    {
	if (NaturalDigits(u)[i])
	{
	    b = DigitBmod (NaturalDigits(u)[i], v0, DIGITBITS);
	    NaturalDigitMultiply (v, b, t);
	    if (NaturalGreaterEqualOffset (u, t, i))
		NaturalSubtractOffset (u, t, i);
	    else
		NaturalSubtractOffsetReverse (u, t, i);
	    d = NaturalWidth (u) - NaturalWidth (v) - (i << LLBASE2) + 1;
	}
	i++;
	d -= DIGITBITS;
	STEP;
    }
    if (d == DIGITBITS)
	dmask = MAXDIGIT;
    else
	dmask = (((digit) 1) << d) - 1;
    if (NaturalDigits(u)[i] & dmask)
    {
	b = DigitBmod (NaturalDigits(u)[i], v0, d);
	NaturalDigitMultiply (v, b, t);
	if (NaturalGreaterEqualOffset (u, t, i))
	    NaturalSubtractOffset (u, t, i);
	else
	    NaturalSubtractOffsetReverse (u, t, i);
	STEP;
    }
    FINISH("bdivmod");
    RETURN (NaturalRsl (u, (i << LLBASE2) + d));;
}

#ifdef CHECK
static Natural *
RegularGcd (Natural *u, Natural *v)
{
    ENTER ();
    Natural	*quo, *rem;

    while (v->length) 
    {
	quo = NaturalDivide (u, v, &rem);
	u = v;
	v = rem;
    }
    RETURN (u);
}
#endif

#define Odd(n)	(NaturalDigits(n)[0] & 1)

static int
NaturalZeroBits (Natural *u)
{
    digit   *ut = NaturalDigits (u);
    digit   d;
    int	    z = 0;

    while ((d = *ut++) == 0)
	z += LBASE2;
    while ((d & 1) == 0)
    {
	z++;
	d >>= 1;
    }
    return z;
}

Natural *
NaturalGcd (Natural *u, Natural *v)
{
    ENTER ();
#ifdef CHECK
    Natural	*true = RegularGcd (u, v);
#endif
    Natural	*t;
    int		normal;
    int		u_zeros, v_zeros;

    u_zeros = NaturalZeroBits (u);
    v_zeros = NaturalZeroBits (v);
    normal = u_zeros;
    if (u_zeros > v_zeros)
	normal = v_zeros;
    u = NaturalRsl (u, u_zeros);
    v = NaturalRsl (v, v_zeros);
    if (NaturalLess (u, v))
    {
	t = u;
	u = v;
	v = t;
    }
#if 0
    if (NaturalLength (u) > NaturalLength (v) + 1)
    {
	u = NaturalBdivmod (u, v);
	if (NaturalZero (u))
	    RETURN (v);
    }
#endif
    if (NaturalLength (u) == 1 && NaturalLength (v) == 1)
    {
	digit	ud = NaturalDigits(u)[0];
	digit	vd = NaturalDigits(v)[0];
	digit	td;
	START;

	while (vd)
	{
	    while (!(vd&1))
		vd >>= 1;
	    if (vd < ud)
	    {
		td = ud;
		ud = vd;
		vd = td;
	    }
	    vd -= ud;
	    STEP;
	}
	NaturalDigits(u)[0] = ud;
	FINISH ("gcd1");
    }
    else if (NaturalLength (u) <= 2 && NaturalLength (v) <= 2)
    {
	double_digit	ud;
	double_digit	vd;
	double_digit	td;
	START;
	
	ud = NaturalDigits(u)[0];
	if (NaturalLength (u) == 2)
	    ud |= ((double_digit) NaturalDigits(u)[1]) << LBASE2;
	vd = NaturalDigits(v)[0];
	if (NaturalLength (v) == 2)
	    vd |= ((double_digit) NaturalDigits(v)[1]) << LBASE2;
	while (vd)
	{
	    while (!(vd&1))
		vd >>= 1;
	    if (vd < ud)
	    {
		td = ud;
		ud = vd;
		vd = td;
	    }
	    vd -= ud;
	    STEP;
	}
	td = ud >> LBASE2;
	if (td)
	{
	    if (NaturalLength (u) != 2)
		u = v;
	    NaturalDigits(u)[1] = (digit) td;
	    NaturalLength (u) = 2;
	}
	else
	    NaturalLength (u) = 1;
	NaturalDigits(u)[0] = (digit) ud;
	FINISH ("gcd2");
    }
    else
    {
	START;
	while (v->length)
	{
	    v_zeros = NaturalZeroBits (v);
	    if (v_zeros)
		gcd_right_shift (v, v_zeros);
	    if (NaturalLess (v, u))
	    {
		t = u;
		u = v;
		v = t;
	    }
	    NaturalSubtractOffset (v, u, 0);
	    STEP;
	}
	FINISH ("gcd");
    }
    u = NaturalLsl (u, normal);
#ifdef CHECK
    if (!NaturalEqual (u, true))
	printf ("bad gcd\n");
#endif
    RETURN (u);
}
