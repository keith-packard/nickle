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

static void
gcd_subtract (Natural *a, Natural *b)
{
    int		    index;
    digit	    carry;
    digit	    *at, *bt;
    digit	    av, bv;
    int		    len;

    carry = 0;
    at = NaturalDigits(a);
    bt = NaturalDigits(b);
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
    if (carry && a->length > b->length)
    {
	*at = *at - carry;
	carry = 0;
    }
    len = a->length;
    at = NaturalDigits(a) + len;
    while (len > 0 && *--at == 0)
	len--;
    a->length = len;
}

#ifdef CHECK
Natural *
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

Natural *
NaturalGcd (u, v)
    Natural	*u, *v;
{
    ENTER ();
#ifdef CHECK
    Natural	*true = RegularGcd (u, v);
#endif
    Natural	*t;
    digit	*ut, *vt;
    digit	mask;
    digit	match;
    int		i;
    int		normal;

    ut = NaturalDigits (u);
    vt = NaturalDigits (v);
    for (normal = 0; (match = (*ut++|*vt++)) == 0; normal += LBASE2)
	;
    mask = 1;
    while (!(match & mask))
    {
	mask <<= 1;
	normal++;
    }
    u = NaturalRsl (u, normal);
    v = NaturalRsl (v, normal);
    if (NaturalLength (u) == 1 && NaturalLength (v) == 1)
    {
	digit	ud = NaturalDigits(u)[0];
	digit	vd = NaturalDigits(v)[0];
	digit	td;

	if (vd & 1)
	{
	    td = ud;
	    ud = vd;
	    vd = td;
	}
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
	}
	NaturalDigits(u)[0] = ud;
    }
    else if (NaturalLength (u) <= 2 && NaturalLength (v) <= 2)
    {
	double_digit	ud;
	double_digit	vd;
	double_digit	td;
	
	ud = NaturalDigits(u)[0];
	if (NaturalLength (u) == 2)
	    ud |= ((double_digit) NaturalDigits(u)[1]) << LBASE2;
	vd = NaturalDigits(v)[0];
	if (NaturalLength (v) == 2)
	    vd |= ((double_digit) NaturalDigits(v)[1]) << LBASE2;
	if (vd & 1)
	{
	    td = ud;
	    ud = vd;
	    vd = td;
	}
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
    }
    else
    {
	if (Odd (v))
	{
	    t = u;
	    u = v;
	    v = t;
	}
	while (v->length)
	{
	    vt = NaturalDigits (v);
	    for (i = 0; (match = *vt++) == 0; i += LBASE2)
		;
	    mask = 1;
	    while (!(match & mask))
	    {
		mask <<= 1;
		i++;
	    }
	    if (i)
		gcd_right_shift (v, i);
	    if (NaturalLess (v, u))
	    {
		t = u;
		u = v;
		v = t;
	    }
	    gcd_subtract (v, u);
	}
    }
    u = NaturalLsl (u, normal);
#ifdef CHECK
    if (!NaturalEqual (u, true))
	printf ("bad gcd\n");
#endif
    RETURN (u);
}
