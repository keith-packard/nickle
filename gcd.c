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

int
DoubleDigitWidth (double_digit i)
{
    int	w;

    w = 0;
    while (i)
    {
	w++;
	i >>= 1;
    }
    return w;
}

static int
DigitWidth (digit i)
{
    int	w;

    w = 0;
    while (i)
    {
	w++;
	i >>= 1;
    }
    return w;
}


/* #define CHECK */
/* #define DEBUG */
/* #define STATS */
#ifdef STATS
#define START	    int steps = 0
#define STEP	    steps++
#define FINISH(op)  FilePrintf (FileStdout, "%s %d steps\n", op, steps), steps = 0
#else
#define START
#define STEP
#define FINISH(op)
#endif
#ifdef DEBUG
#define GcdCheckPointer(a,b,c) MemCheckPointer(a,b,c)
#else
#define GcdCheckPointer(a,b,c)
#endif

static void
NaturalBdivmodStepInplace (Natural *u, int offset, Natural *v, int bits)
{
    double_digit    quo;
    digit	    carry_mul;
    digit	    carry_sub;
    digit	    quo_d;
    digit	    d;
    digit	    b;
    digit	    *vd = NaturalDigits(v);
    digit	    *ud = NaturalDigits(u) + offset;
    int		    i;
    
    b = DigitBmod (*ud, *vd, bits);

    carry_mul = 0;
    carry_sub = 0;
    for (i = 0; i < NaturalLength (v); i++)
    {
	quo = (double_digit) b * (double_digit) *vd++ + carry_mul;
	carry_mul = DivBase (quo);
	quo_d = ModBase (quo) + carry_sub;

	if (quo_d)
	{
	    carry_sub = 0;
	    GcdCheckPointer (u, ud, sizeof (digit));
	    d = *ud;
	    if ((*ud = d - quo_d) > d)
		carry_sub = 1;
	}
	ud++;
    }
    carry_sub = carry_sub + carry_mul;
    while (carry_sub && i + offset < NaturalLength (u))
    {
	quo_d = carry_sub;
	carry_sub = 0;
	GcdCheckPointer (u, ud, sizeof (digit));
	d = *ud;
	if ((*ud++ = d - quo_d) > d)
	    carry_sub = 1;
	i++;
    }
    if (i + offset == NaturalLength (u))
    {
	/*
	 * Two's compliment negative results
	 */
	if (carry_sub)
	{
	    ud = NaturalDigits (u) + offset;
	    while (i--)
	    {
		d = *ud;
		*ud++ = (~d) + carry_sub;
		if (d)
		    carry_sub = 0;
	    }
	}
    }
    ud = NaturalDigits(u) + NaturalLength (u) - 1;
    while (*ud == 0)
    {
        GcdCheckPointer (u, ud, sizeof (digit));
	if (ud-- == NaturalDigits (u))
	    break;
    }
    NaturalLength (u) = (ud - NaturalDigits (u)) + 1;
}

static void
NaturalBdivmodInplace (Natural *u, Natural *v)
{
    ENTER ();
    digit   v0;
    int	    i;
    int	    d;
    digit   dmask;
    
    v0 = NaturalDigits(v)[0];
    i = 0;
    d = NaturalWidth (u) - NaturalWidth (v) + 1;
    while (d > DIGITBITS)
    {
#ifdef DEBUG
	FilePrintf (FileStdout, "i %d u[i] %x\n", i, NaturalDigits(u)[i]);
#endif
	if (i < NaturalLength (u) && NaturalDigits(u)[i])
	{
	    NaturalBdivmodStepInplace (u, i, v, DIGITBITS);
#ifdef DEBUG
	    FilePrintf (FileStdout, "i %d u %N\n", i, u);
#endif
	}
	i++;
	d -= DIGITBITS;
    }
    if (d == DIGITBITS)
	dmask = MAXDIGIT;
    else
	dmask = (((digit) 1) << d) - 1;
    if (NaturalDigits(u)[i] & dmask)
    {
        NaturalBdivmodStepInplace (u, i, v, d);
    }
    if (NaturalLength (u))
	gcd_right_shift (u, (i << LLBASE2) + d);
    EXIT ();
}

Natural *
NaturalBdivmod (Natural *u_orig, Natural *v)
{
    ENTER ();
    Natural *u;
    
    u = AllocNatural (u_orig->length);
    NaturalCopy (u_orig, u);
    NaturalBdivmodInplace (u, v);
    RETURN (u);
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

    if (u->length)
    {
	while ((d = *ut++) == 0)
	    z += LBASE2;
	while ((d & 1) == 0)
	{
	    z++;
	    d >>= 1;
	}
    }
    return z;
}

static void
ReducedRatMod (Natural *x, Natural *y, digit *np, digit *dp)
{
    digit   n1, n2, nt, n2i;
    digit   d;
    digit   c;
    int	    w2;

    c = DigitBmod (NaturalDigits(x)[0], NaturalDigits(y)[0], DIGITBITS);

    n1 = c;
    n2 = -n1;

    while ((w2 = DigitWidth (n2)) > DIGITBITS/2)
    {
	int i = DigitWidth (n1) - w2;
	while (n1 >= n2)
	{
	    if (n1 >= (n2i = n2 << i))
		n1 = n1 - n2i;
	    i--;
	}
	nt = n1;
	n1 = n2;
	n2 = nt;
    }
    *np = n2;
    d = DigitBmod (n2, c, DIGITBITS/2 + 1);
    *dp = d;
#ifdef DEBUG
    FilePrintf (FileStdout, "u %n v %n\n", x, y);
    FilePrintf (FileStdout, "n %u d %u\n", *np, *dp);
#endif
}

static void
NaturalKaryReductionInplace (Natural *u, Natural *v)
{
    ENTER ();
    Natural	    *t;
    digit	    n, d;
    digit	    *ud, *vd, *rd;
    double_digit    qd, qv;
    digit	    carry_d, carry_v, carry;
    digit	    quo_d, quo_v, r;
    int		    i;
    Bool	    add;
    
    if (NaturalLength (u) < NaturalLength (v))
    {
	t = u;
	u = v;
	v = t;
    }
    ReducedRatMod (u, v, &n, &d);
    add = False;
    if (d & (1 << DIGITBITS/2))
    {
	add = True;
	d = -d & ((1 << DIGITBITS/2) - 1);
#ifdef DEBUG
	FilePrintf (FileStdout, "d changed to %d\n", d);
#endif
    }
    ud = NaturalDigits (u);
    vd = NaturalDigits (v);
    rd = NaturalDigits (u);
    carry = 0;
    carry_d = 0;
    carry_v = 0;
    for (i = 0; i < NaturalLength (v); i++)
    {
	GcdCheckPointer (u, ud, sizeof (digit));
	qd      = (double_digit) d * (double_digit) *ud++ + carry_d;
	carry_d = DivBase (qd);
	quo_d   = ModBase (qd);
	
	GcdCheckPointer (v, vd, sizeof (digit));
	qv      = (double_digit) n * (double_digit) *vd++ + carry_v;
	carry_v = DivBase (qv);
	quo_v   = ModBase (qv);
	
        quo_v   = quo_v + carry;
	
        if (quo_v)
        {
	    carry = 0;
	    if (add)
	    {
		if ((r = quo_d + quo_v) < quo_d)
		    carry = 1;
	    }
	    else
	    {
		if ((r = quo_d - quo_v) > quo_d)
		    carry = 1;
	    }
	}
	else
	    r = quo_d;
	GcdCheckPointer (u, rd, sizeof (digit));
	if (i)
	    *rd++ = r;
    }
    for (; i < NaturalLength (u); i++)
    {
	GcdCheckPointer (u, ud, sizeof (digit));
	qd      = (double_digit) d * (double_digit) *ud++ + carry_d;
	carry_d = DivBase (qd);
	quo_d   = ModBase (qd);

	quo_v = carry_v + carry;
	carry_v = 0;

	if (quo_v)
	{
	    carry = 0;
	    if (add)
	    {
		if ((r = quo_d + quo_v) < quo_d)
		    carry = 1;
	    }
	    else
	    {
		if ((r = quo_d - quo_v) > quo_d)
		    carry = 1;
	    }
	}
	else
	    r = quo_d;
	GcdCheckPointer (u, rd, sizeof (digit));
	*rd++ = r;
    }
    quo_d = carry_d;
    quo_v = carry_v + carry;

    if (quo_v)
    {
	carry = 0;
	if (add)
	{
	    if ((r = quo_d + quo_v) < quo_d)
		carry = 1;
	}
	else
	{
	    if ((r = quo_d - quo_v) > quo_d)
		carry = 1;
	}
    }
    else
	r = quo_d;
    *rd++ = r;
    if (carry)
    {
	if (add)
	    abort ();
	else
	{
	    i = rd - NaturalDigits (u);
	    rd = NaturalDigits (u);
	    while (i--)
	    {
		r = *rd;
		*rd++ = (~r) + carry;
		if (r)
		    carry = 0;
	    }
	}
    }
    
    i = rd - NaturalDigits (u);
    while (i > 0 && *--rd == 0)
	i--;
    NaturalLength (u) = i;
#ifdef DEBUG
    FilePrintf (FileStdout, "Reduction result %n\n", u);
#endif
    EXIT ();
}

Natural *
NaturalKaryReduction (Natural *u, Natural *v)
{
    ENTER ();
    Natural *t;
    
    if (NaturalLength (u) < NaturalLength (v))
    {
	t = u;
	u = v;
	v = t;
    }
    t = AllocNatural (NaturalLength (u));
    NaturalCopy (u, t);
    NaturalKaryReductionInplace (t, v);
    RETURN (t);
}

Natural *
NaturalGcd (Natural *u0, Natural *v0)
{
    ENTER ();
#ifdef CHECK
    Natural	*true = RegularGcd (u0, v0);
#endif
    Natural	*u, *v, *t;
    int		normal;
    int		u_zeros, v_zeros;
    int		fix;

    u_zeros = NaturalZeroBits (u0);
    v_zeros = NaturalZeroBits (v0);
    normal = u_zeros;
    if (u_zeros > v_zeros)
	normal = v_zeros;
    u = NaturalRsl (u0, u_zeros);
    v = NaturalRsl (v0, v_zeros);
    if (NaturalLess (u, v))
    {
	t = u;
	u = v;
	v = t;
    }
#if 0
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
#endif
    {
	START;

#if 0
	NaturalBdivmodInplace (u, v);
	if (NaturalZero (u))
	    RETURN (NaturalLsl (v, normal));
#endif
	while (v->length)
	{
	    if (aborting)
		RETURN (One);
	    if (NaturalWidth (u) - NaturalWidth (v) > 10)
		NaturalBdivmodInplace (u, v);
	    else
		NaturalKaryReductionInplace (u, v);
	    u_zeros = NaturalZeroBits (u);
	    if (u_zeros)
		gcd_right_shift (u, u_zeros);
	    t = u;
	    u = v;
	    v = t;
	    STEP;
	}
#ifdef DEBUG
	FilePrintf (FileStdout, "After weber:\n u: %n\n v: %n\n", u, v);
#endif
	FINISH ("gcd_weber");
	for (fix = 0; fix < 2; fix++)
	{
	    v = u;
	    if (fix)
		u = v0;
	    else
		u = u0;
	    u = NaturalBdivmod (u, v);
	    if (NaturalZero (u))
	    {
		u = v;
		continue;
	    }
#ifdef DEBUG
	    FilePrintf (FileStdout, " fix %d\n u = %n;\n v = %n;\n", fix, u, v);
#endif
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
#ifdef DEBUG
	    FilePrintf (FileStdout, " fix %n\n", u);
#endif
	}
	FINISH ("gcd_fix");
    }
    u = NaturalLsl (u, normal);
#ifdef CHECK
    if (!NaturalEqual (u, true))
    {
	FilePrintf (FileStdout, "gcd failure:\n");
	FilePrintf (FileStdout, "    u: %n\n    v: %n\n  gcd: %n\n  got: %n\n",
		    u0, v0, true, u);
    }
#endif
    RETURN (u);
}
