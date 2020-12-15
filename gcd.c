/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * gcd.c
 *
 * compute gcd of two natural numbers
 *
 * The Accelerated Integer GCD Algorithm
 *
 * Kenneth Weber
 * Kent State University
 * ACM Transactions on Mathematical Software, Vol 21, No. 1 March 1995, Pages 111-122
 */

#include	"nickle.h"

#undef CHECK
#undef DEBUG_BMOD
#undef DEBUG_BDIVMOD
#undef DEBUG_GCD
#undef DEBUG_KARY
#undef DEBUG_RATMOD
#undef DEBUG_FIX
#undef DEBUG_POINTERS

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
#ifdef DEBUG_POINTERS
#define GcdCheckPointer(a,b,c) MemCheckPointer(a,b,c)
#else
#define GcdCheckPointer(a,b,c)
#endif

static void
NaturalRslInplace (Natural *v, int shift)
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
    if (length > 0 && (NaturalDigits(v)[v->length - 1] >> shift) == 0)
    {
	length--;
	last = 0;
    }
    if (length <= 0)
    {
	v->length = 0;
	return;
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
    digit   smask;
    digit   m = 0;

#ifdef DEBUG_BMOD
    FilePrintf (FileStdout, "DigitBmod (%u, %u, %d)\n", u, v, s);
#endif
    if (s == DIGITBITS)
	smask = MAXDIGIT;
    else
	smask = (1 << s) - 1;
    for (i = 0; i < s; i++)
    {
	umask |= imask;
	if (u & umask)
	{
	    u = (u - v) & smask;
	    m = m | imask;
	}
	imask <<= 1;
	v <<= 1;
    }
#ifdef DEBUG_BMOD
    FilePrintf (FileStdout, "  = %u\n", m);
#endif
    return m;
}

static int
DigitWidth (digit i)
#if DIGITBITS == 32
{
    int	    w;

    w = 0;
    if (i & 0xffff0000)
	w += 16;
    else
	i <<= 16;
    if (i & 0xff000000)
	w += 8;
    else
	i <<= 8;
    if (i & 0xf0000000)
	w += 4;
    else
	i <<= 4;
    if (i & 0xc0000000)
	w += 2;
    else
	i <<= 2;
    if (i & 0x80000000)
	w++;
    return w;
}
#endif
#if DIGITBITS == 16
{
    int	    w;

    w = 0;
    if (i & 0xff00)
	w += 8;
    else
	i <<= 8;
    if (i & 0xf000)
	w += 4;
    else
	i <<= 4;
    if (i & 0xc000)
	w += 2;
    else
	i <<= 2;
    if (i & 0x8000)
	w++;
    return w;
}
#endif

int
NaturalWidth (Natural *u)
{
    int	    w;
    
    if (NaturalZero (u))
	return 0;
    w = NaturalLength (u) - 1;
    return DigitWidth (NaturalDigits (u)[w]) + (w << LLBASE2);
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


/*
 * Return multiplicative inverse of d in 2**DIGITBITS
 *
 * This is just d ** (2**DIGITBITS-1)
 */

static digit
DigitInverse (digit d)
{
    digit   b = MAXDIGIT;
    digit   result = 1;
    
    for (;;)
    {
        result = result * d;
	b >>= 1;
	if (!b)
	    break;
	d = d * d;
    }
    return result;
}

static void
NaturalBdivmodStepInplace (Natural *u, Natural *v, digit b, Bool shift)
{
    double_digit    quo;
    digit	    carry_mul;
    digit	    carry_sub;
    digit	    quo_d;
    digit	    d;
    digit	    r;
    digit	    *vd = NaturalDigits(v);
    digit	    *ud = NaturalDigits(u);
    digit    	    *rd = ud;
    int		    i;

    carry_mul = 0;
    carry_sub = 0;
    for (i = 0; i < NaturalLength (v); i++)
    {
	quo = (double_digit) b * (double_digit) *vd++ + carry_mul;
	carry_mul = DivBase (quo);
	quo_d = ModBase (quo) + carry_sub;

	d = *ud++;
	if (quo_d)
	{
	    carry_sub = 0;
	    GcdCheckPointer (u, ud, sizeof (digit));
	    if ((r = d - quo_d) > d)
		carry_sub = 1;
	}
	else
	    r = d;
	if (!shift || i)
	    *rd++ = r;
    }
    carry_sub = carry_sub + carry_mul;
    while (carry_sub && i < NaturalLength (u))
    {
	quo_d = carry_sub;
	carry_sub = 0;
	GcdCheckPointer (u, ud, sizeof (digit));
	d = *ud++;
	if ((r = d - quo_d) > d)
	    carry_sub = 1;
	if (!shift || i)
	    *rd++ = r;
	i++;
    }
    while (i < NaturalLength (u))
    {
	GcdCheckPointer (u, ud, sizeof (digit));
	r = *ud++;
	if (!shift || i)
	    *rd++ = r;
	i++;
    }
    /*
     * The caller must ensure that an extra digit space
     * is available for this operation
     */
    if (carry_sub)
    {
	quo_d = carry_sub;
	carry_sub = 0;
	GcdCheckPointer (u, ud, sizeof (digit));
	d = 0;
	if ((r = d - quo_d) > d)
	    carry_sub = 1;
	if (!shift || i)
	    *rd++ = r;
	i = rd - NaturalDigits(u);
	/*
	 * Two's compliment negative results
	 */
	carry_sub = 1;
	rd = NaturalDigits (u);
	while (i--)
	{
	    d = *rd;
	    *rd++ = (~d) + carry_sub;
	    if (d)
		carry_sub = 0;
	}
    }
    /*
     * Trim leading zeros
     */
    while (--rd >= NaturalDigits (u) && *rd == 0)
	;
    u->length = ((rd + 1) - NaturalDigits (u));
}

static void
NaturalBdivmodInplace (Natural *u, Natural *v, int bits)
{
    digit   v0_inv = DigitInverse (NaturalDigits(v)[0]);
    digit   q0;
    
#ifdef DEBUG_BDIVMOD
    FilePrintf (FileStdout, "v0 %x v0_inv %x\n", NaturalDigits(v)[0], v0_inv);
#endif
    while (bits >= DIGITBITS)
    {
	q0 = NaturalDigits(u)[0] * v0_inv;

#ifdef DEBUG_BDIVMOD
	FilePrintf (FileStdout, "u[0] %x q0 %x\n",
		    NaturalDigits(u)[0], q0);
#endif
	if (q0)
	    NaturalBdivmodStepInplace (u, v, q0, True);
	else
	    NaturalRslInplace (u, DIGITBITS);
#ifdef DEBUG_BDIVMOD
	FilePrintf (FileStdout, "bits %d u %N\n", bits, u);
#endif
	bits -= DIGITBITS;
    }
    if (bits)
    {
	digit	dmask = ((digit) 1 << bits) - 1;
	
	q0 = ((NaturalDigits(u)[0] & dmask) * v0_inv) & dmask;
#ifdef DEBUG_BDIVMOD
	FilePrintf (FileStdout, "u[0] %x q0 %x dmask %x\n",
		    NaturalDigits(u)[0], q0, dmask);
#endif
	if (q0)
	    NaturalBdivmodStepInplace (u, v, q0, False);
	NaturalRslInplace (u, bits);
#ifdef DEBUG_BDIVMOD
	FilePrintf (FileStdout, "bits %d u %N\n", bits, u);
#endif
    }
}

Natural *
NaturalBdivmod (Natural *u_orig, Natural *v)
{
    ENTER ();
    Natural *u;
    
    u = AllocNatural (u_orig->length + 2);
    NaturalCopy (u_orig, u);
    NaturalBdivmodInplace (u, v, NaturalWidth (u) - NaturalWidth (v) + 1);
    RETURN (u);
}

#if 0
static void
oldNaturalBdivmodStepInplace (Natural *u, Natural *v, int bits, Bool shift)
{
    double_digit    quo;
    digit	    carry_mul;
    digit	    carry_sub;
    digit	    quo_d;
    digit	    d;
    digit	    b;
    digit	    r;
    digit	    *vd = NaturalDigits(v);
    digit	    *ud = NaturalDigits(u);
    digit    	    *rd = ud;
    int		    i;
    
    b = DigitBmod (*ud, *vd, bits);

    carry_mul = 0;
    carry_sub = 0;
    for (i = 0; i < NaturalLength (v); i++)
    {
	quo = (double_digit) b * (double_digit) *vd++ + carry_mul;
	carry_mul = DivBase (quo);
	quo_d = ModBase (quo) + carry_sub;

        d = *ud++;
	if (quo_d)
	{
	    carry_sub = 0;
	    GcdCheckPointer (u, ud, sizeof (digit));
	    if ((r = d - quo_d) > d)
		carry_sub = 1;
	}
	else
	    r = d;
	if (!shift || i)
	    *rd++ = r;
    }
    carry_sub = carry_sub + carry_mul;
    while (carry_sub && i < NaturalLength (u))
    {
	quo_d = carry_sub;
	carry_sub = 0;
	GcdCheckPointer (u, ud, sizeof (digit));
	d = *ud++;
	if ((r = d - quo_d) > d)
	    carry_sub = 1;
	if (!shift || i)
	    *rd++ = r;
	i++;
    }
    while (i < NaturalLength (u))
    {
	r = *ud++;
	if (!shift || i)
	    *rd++ = r;
	i++;
    }
    /*
     * The caller must ensure that an extra digit space
     * is available for this operation
     */
    if (carry_sub)
    {
	quo_d = carry_sub;
	carry_sub = 0;
	GcdCheckPointer (u, ud, sizeof (digit));
	d = 0;
	if ((r = d - quo_d) > d)
	    carry_sub = 1;
	if (!shift || i)
	    *rd++ = r;
	i = rd - NaturalDigits(u);
	/*
	 * Two's compliment negative results
	 */
	carry_sub = 1;
	rd = NaturalDigits (u);
	while (i--)
	{
	    d = *rd;
	    *rd++ = (~d) + carry_sub;
	    if (d)
		carry_sub = 0;
	}
    }
    while (*--rd == 0)
	if (rd == NaturalDigits (u))
	{
	    NaturalLength (u) = 0;
	    return;
	}
    NaturalLength (u) = (rd - NaturalDigits (u)) + 1;
}

static void
oldNaturalBdivmodInplace (Natural *u, Natural *v, int d)
{
    ENTER ();
    digit   v0;
    digit   dmask;
    
    v0 = NaturalDigits(v)[0];
    while (d >= DIGITBITS)
    {
#ifdef DEBUG_BDIVMOD
	FilePrintf (FileStdout, "u[0] %x\n", NaturalDigits(u)[0]);
#endif
	if (NaturalDigits(u)[0])
	{
	    oldNaturalBdivmodStepInplace (u, v, DIGITBITS, True);
#ifdef DEBUG_BDIVMOD
	    FilePrintf (FileStdout, "d %d u %N\n", d, u);
#endif
	}
	else
	{
	    NaturalRslInplace (u, DIGITBITS);
	}
	d -= DIGITBITS;
    }
    if (d)
    {
	dmask = (((digit) 1) << d) - 1;
	if (NaturalDigits(u)[0] & dmask)
	{
	    oldNaturalBdivmodStepInplace (u, v, d, False);
#ifdef DEBUG_BDIVMOD
	    FilePrintf (FileStdout, "d %d u %N\n", d, u);
#endif
	}
	if (NaturalLength (u))
	    NaturalRslInplace (u, d);
    }
#ifdef DEBUG_BDIVMOD
    FilePrintf (FileStdout, "result u %N (shift %d)\n", u, d);
#endif
    EXIT ();
}

static Natural *
oldNaturalBdivmod (Natural *u_orig, Natural *v)
{
    ENTER ();
    Natural *u;
    
    u = AllocNatural (u_orig->length + 2);
    NaturalCopy (u_orig, u);
    oldNaturalBdivmodInplace (u, v, NaturalWidth (u) - NaturalWidth (v) + 1);
    RETURN (u);
}

#define NaturalBdivmod	oldNaturalBdivmod
#define NaturalBdivmodInplace oldNaturalBdivmodInplace
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
    digit   d1, d2, dt;
    digit   c;
    int	    w2;

    c = DigitBmod (NaturalDigits(x)[0], NaturalDigits(y)[0], DIGITBITS);

    n1 = c;
    d1 = 1;
    n2 = -n1;
    d2 = -1;

    while ((w2 = DigitWidth (n2)) > DIGITBITS/2)
    {
	int i = DigitWidth (n1) - w2;
	while (n1 >= n2)
	{
	    if (n1 >= (n2i = n2 << i))
	    {
		n1 = n1 - n2i;
		d1 = d1 - (d2 << i);
	    }
	    i--;
	}
	nt = n1;
	n1 = n2;
	n2 = nt;
	
	dt = d1;
	d1 = d2;
	d2 = dt;
#ifdef DEBUG_RATMOD
	FilePrintf (FileStdout, "n1 %d n2 %d d1 %d d2 %d\n", n1, n2, d1, d2);
#endif
    }
    *np = n2;
    *dp = d2;
#ifdef DEBUG_RATMOD
    FilePrintf (FileStdout, "u %n v %n\n", x, y);
    FilePrintf (FileStdout, "c %u n %u d %u/%u\n", c, *np, *dp, d2);
#endif
}

static void
NaturalKaryReductionInplace (Natural *u, Natural *v)
{
    ENTER ();
    digit	    n, d;
    digit	    *ud, *vd, *rd;
    double_digit    qd, qv;
    digit	    carry_d, carry_v, carry;
    digit	    quo_d, quo_v, r;
    int		    i;
    Bool	    add;
    int		    lim;
    
    ReducedRatMod (u, v, &n, &d);
    add = False;
    if (d & (1 << DIGITBITS/2))
    {
	add = True;
	d = -d & ((1 << DIGITBITS/2) - 1);
#ifdef DEBUG_KARY
	FilePrintf (FileStdout, "d changed to %d\n", d);
#endif
    }
    ud = NaturalDigits (u);
    vd = NaturalDigits (v);
    rd = NaturalDigits (u);
    carry = 0;
    carry_d = 0;
    carry_v = 0;
    lim = NaturalLength (v);
    if (lim > NaturalLength (u))
	lim = NaturalLength (u);
    for (i = 0; i < lim; i++)
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
	if (i)
	    *rd++ = r;
    }
    for (; i < NaturalLength (v); i++)
    {
	quo_d = carry_d;
	carry_d = 0;

	GcdCheckPointer (v, vd, sizeof (digit));
	qv = (double_digit) n * (double_digit) *vd++ + carry_v;
	carry_v = DivBase (qv);
	quo_v = ModBase (qv);

	quo_v = quo_v + carry;
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
	    /*
	     * Two's compliment negative result
	     */
	    i = rd - NaturalDigits (u);
	    rd = NaturalDigits (u);
	    carry = 1;
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
    u->length = i;
#ifdef DEBUG_KARY
    FilePrintf (FileStdout, "Reduction result %n\n", u);
#endif
    EXIT ();
}

Natural *
NaturalKaryReduction (Natural *u, Natural *v)
{
    ENTER ();
    Natural *t;
    int	    len;
    
    len = NaturalLength (u);
    if (len < NaturalLength (v))
	len = NaturalLength (v);
    t = AllocNatural (len + 1);
    NaturalCopy (u, t);
    NaturalKaryReductionInplace (t, v);
    RETURN (t);
}

/*
 * Allocate space and initialize for GCD.  Extra space is needed for the
 * rational reduction step
 */

static Natural *
NaturalGcdNormalize (Natural *u, int shift)
{
    ENTER ();
    Natural *r;
    int	    dshift;
    digit   d1, d2;
    digit   *ut, *rt;
    int	    index;

    dshift = shift >> LLBASE2;
    shift = shift & (LBASE2 - 1);
    r = AllocNatural (NaturalLength (u) - dshift + 2);
    rt = NaturalDigits (r);
    ut = NaturalDigits (u) + dshift;
    index = NaturalLength (u) - dshift;
    if (shift)
    {
	d2 = *ut++;
	while (--index)
	{
	    d1 = d2;
	    d2 = *ut++;
	    *rt++ = (d1 >> shift) | (d2 << (LBASE2 - shift));
	}
        *rt++ = (d2 >> shift);
    }
    else
    {
	while (index--)
	    *rt++ = *ut++;
    }
    while (*--rt == 0)
	;
    r->length = rt - NaturalDigits (r) + 1;
    RETURN (r);
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

    if (NaturalZero (u0))
	RETURN (zero_natural);
    if (NaturalZero (v0))
	RETURN (zero_natural);
    u_zeros = NaturalZeroBits (u0);
    v_zeros = NaturalZeroBits (v0);
    normal = u_zeros;
    if (u_zeros > v_zeros)
	normal = v_zeros;
    u = NaturalGcdNormalize (u0, u_zeros);
    v = NaturalGcdNormalize (v0, v_zeros);
    if (NaturalLess (u, v))
    {
	t = u;
	u = v;
	v = t;
    }
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
	    u->length = 2;
	}
	else
	    u->length = 1;
	NaturalDigits(u)[0] = (digit) ud;
	FINISH ("gcd2");
    }
    else
    {
	START;

	while (v->length)
	{
	    if (aborting)
		RETURN (one_natural);
#ifdef DEBUG_GCD
	    FilePrintf (FileStdout, "u = %n;\n", u);
	    FilePrintf (FileStdout, "v = %n;\n", v);
#endif
	    if (NaturalWidth (u) - NaturalWidth (v) > 10)
	    {
#ifdef DEBUG_GCD
		FilePrintf (FileStdout, "bdivmod\n");
#endif
		NaturalBdivmodInplace (u, v,
				       NaturalWidth (u) - NaturalWidth (v) + 1);
	    }
	    else
	    {
#ifdef DEBUG_GCD
		FilePrintf (FileStdout, "kary\n");
#endif
		NaturalKaryReductionInplace (u, v);
	    }
	    u_zeros = NaturalZeroBits (u);
	    if (u_zeros)
		NaturalRslInplace (u, u_zeros);
	    t = u;
	    u = v;
	    v = t;
	    STEP;
	}
	FINISH ("gcd_weber");
#ifdef DEBUG_FIX
	FilePrintf (FileStdout, "After weber:\n u: %n\n v: %n\n", u, v);
#endif
	for (fix = 0; fix < 2; fix++)
	{
	    v = u;
	    if (fix)
		u = v0;
	    else
		u = u0;
#ifdef DEBUG_FIX
	    FilePrintf (FileStdout, " fix %d\n u = %n;\n v = %n;\n", fix, u, v);
#endif
	    u = NaturalBdivmod (u, v);
	    if (NaturalZero (u))
	    {
		u = v;
		continue;
	    }
	    while (v->length)
	    {
		v_zeros = NaturalZeroBits (v);
		if (v_zeros)
		    NaturalRslInplace (v, v_zeros);
		if (NaturalLess (v, u))
		{
		    t = u;
		    u = v;
		    v = t;
		}
		NaturalSubtractOffset (v, u, 0);
		STEP;
	    }
#ifdef DEBUG_FIX
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
