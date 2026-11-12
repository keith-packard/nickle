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

#include "nickle.h"
#include <math.h>

int
RationalInit(void)
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
RationalPlusHelper(Sign sign, Rational *a, Rational *b)
{
    ENTER();
    RETURN(NewRational(sign,
                       NaturalPlus(NaturalTimes(a->num, b->den), NaturalTimes(b->num, a->den)),
                       NaturalTimes(a->den, b->den)));
}

static Value
RationalMinusHelper(Rational *a, Rational *b)
{
    ENTER();
    Natural *ra, *rb, *t;
    Sign     sign = Positive;

    ra = NaturalTimes(a->num, b->den);
    rb = NaturalTimes(b->num, a->den);
    if (NaturalLess(ra, rb)) {
        sign = Negative;
        t = ra;
        ra = rb;
        rb = t;
    }
    RETURN(NewRational(sign, NaturalMinus(ra, rb), NaturalTimes(a->den, b->den)));
}

static Value
RationalPlus(Value av, Value bv, int expandOk)
{
    ENTER();
    Rational *a = &av->rational, *b = &bv->rational;
    Value     ret;

    (void)expandOk;
    switch (catagorize_signs(a->sign, b->sign)) {
    case BothPositive:
    case BothNegative:
        ret = RationalPlusHelper(a->sign, a, b);
        break;
    case FirstPositive:
        ret = RationalMinusHelper(a, b);
        break;
    case SecondPositive:
        ret = RationalMinusHelper(b, a);
        break;
    default:
        abort();
    }
    RETURN(ret);
}

static Value
RationalMinus(Value av, Value bv, int expandOk)
{
    ENTER();
    Rational *a = &av->rational, *b = &bv->rational;
    Value     ret;

    (void)expandOk;
    switch (catagorize_signs(a->sign, b->sign)) {
    case BothPositive:
        ret = RationalMinusHelper(a, b);
        break;
    case FirstPositive:
    case SecondPositive:
        ret = RationalPlusHelper(a->sign, a, b);
        break;
    case BothNegative:
        ret = RationalMinusHelper(b, a);
        break;
    default:
        abort();
    }
    RETURN(ret);
}

static Value
RationalTimes(Value av, Value bv, int expandOk)
{
    ENTER();
    Rational *a = &av->rational, *b = &bv->rational;
    Sign      sign;

    (void)expandOk;
    sign = Positive;
    if (a->sign != b->sign)
        sign = Negative;
    RETURN(NewRational(sign, NaturalTimes(a->num, b->num), NaturalTimes(a->den, b->den)));
}

static Value
RationalDivide(Value av, Value bv, int expandOk)
{
    ENTER();
    Rational *a = &av->rational, *b = &bv->rational;
    Sign      sign;

    (void)expandOk;
    if (NaturalZero(b->num)) {
        RaiseStandardException(exception_divide_by_zero, 2, av, bv);
        RETURN(Void);
    }
    sign = Positive;
    if (a->sign != b->sign)
        sign = Negative;
    RETURN(NewRational(sign, NaturalTimes(a->num, b->den), NaturalTimes(a->den, b->num)));
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
RationalMod(Value av, Value bv, int expandOk)
{
    ENTER();
    Rational *a = &av->rational, *b = &bv->rational;
    Natural  *rem, *div;

    (void)expandOk;
    if (NaturalZero(b->num)) {
        RaiseStandardException(exception_divide_by_zero, 2, av, bv);
        RETURN(Void);
    }
    div = NaturalTimes(b->num, a->den);
    (void)NaturalDivide(NaturalTimes(a->num, b->den), div, &rem);
    if (a->sign == Negative && !NaturalZero(rem))
        rem = NaturalMinus(div, rem);
    RETURN(NewRational(Positive, rem, NaturalTimes(a->den, b->den)));
}

static Value
RationalLess(Value av, Value bv, int expandOk)
{
    ENTER();
    Rational *a = &av->rational, *b = &bv->rational;
    Rational *t;
    Bool      ret;

    (void)expandOk;
    switch (catagorize_signs(a->sign, b->sign)) {
    case BothNegative:
        t = a;
        a = b;
        b = t;
        __NICKLE_FALLTHROUGH;
    case BothPositive:
        if (!NaturalEqual(a->den, b->den))
            ret = NaturalLess(NaturalTimes(a->num, b->den), NaturalTimes(b->num, a->den));
        else
            ret = NaturalLess(a->num, b->num);
        break;
    case FirstPositive:
        ret = False;
        break;
    case SecondPositive:
        ret = True;
        break;
    default:
        abort();
    }
    RETURN(ret ? TrueVal : FalseVal);
}

static Value
RationalEqual(Value av, Value bv, int expandOk)
{
    Rational *a = &av->rational, *b = &bv->rational;

    (void)expandOk;
    if (a->sign == b->sign && NaturalEqual(a->num, b->num) && NaturalEqual(a->den, b->den)) {
        return TrueVal;
    }
    return FalseVal;
}

static Value
RationalNegate(Value av, int expandOk)
{
    ENTER();
    Rational *a = &av->rational;

    (void)expandOk;
    RETURN(NewRational(SignNegate(a->sign), a->num, a->den));
}

static Value
RationalFloor(Value av, int expandOk)
{
    ENTER();
    Rational *a = &av->rational;
    Natural  *quo, *rem;

    (void)expandOk;
    quo = NaturalDivide(a->num, a->den, &rem);
    if (!NaturalZero(rem) && a->sign == Negative)
        quo = NaturalPlus(quo, one_natural);
    RETURN(NewInteger(a->sign, quo));
}

static Value
RationalCeil(Value av, int expandOk)
{
    ENTER();
    Rational *a = &av->rational;
    Natural  *quo, *rem;

    (void)expandOk;
    quo = NaturalDivide(a->num, a->den, &rem);
    if (!NaturalZero(rem) && a->sign == Positive)
        quo = NaturalPlus(quo, one_natural);
    RETURN(NewInteger(a->sign, quo));
}

static Value
RationalPromote(Value av, Value bv)
{
    ENTER();

    (void)bv;
    switch (ValueTag(av)) {
    case rep_int:
        av = NewIntRational(ValueInt(av));
        break;
    case rep_integer:
        av = NewIntegerRational(&av->integer);
        break;
    default:
        break;
    }
    RETURN(av);
}

static Value
RationalReduce(Value av)
{
    ENTER();
    Rational *a = &av->rational;

    if (NaturalEqual(a->den, one_natural))
        av = Reduce(NewInteger(a->sign, a->num));
    RETURN(av);
}

static HashValue
RationalHash(Value av)
{
    Rational *a = &av->rational;

    return NaturalHash(a->den) ^ NaturalHash(a->num) ^ a->sign;
}

/* The most repeating digits that are printed; more are cut off */
#define MAX_SENSIBLE 10000000

/* b * c mod m */
static Natural *
NaturalTimesMod(Natural *b, Natural *c, Natural *m)
{
    ENTER();
    Natural *rem;

    (void)NaturalDivide(NaturalTimes(b, c), m, &rem);
    RETURN(rem);
}

/*
 * Return the number of digits that repeat in the fraction of 1/nden
 * in base 'ibase', which shares no factor with nden: the least k > 0
 * with ibase**k = 1 mod nden. Looking only as far as 'limit' (or
 * MAX_SENSIBLE if 'limit' isn't positive), return -1 if the count is
 * beyond it, or MAX_SENSIBLE + 1 if there is no limit, which the
 * printer cuts off.
 *
 * This is a baby-step giant-step search, which needs no factoring.
 * With m about sqrt(limit), the baby steps store ibase**j for j < m;
 * if none of those is 1, they are all different, and the first giant
 * step ibase**(i*m) to match a stored ibase**j gives the count,
 * i*m - j, as any smaller count would have matched sooner
 */
static int
RationalRepeatLength(int limit, Natural *nden, int ibase)
{
    Natural  *nbase, *v, *step;
    Natural **keys;
    int      *vals;
    size_t    size, mask, h;
    int       bound = limit > 0 ? limit : MAX_SENSIBLE;
    int       m, i, j, found = 0;

    if (NaturalEqual(nden, one_natural))
        return 0;
    ENTER();
    (void)NaturalDivide(NewNatural((double_digit)ibase), nden, &nbase);
    m = (int)ceil(sqrt((double)bound));
    for (size = 2; size < (size_t)m * 2; size *= 2)
        ;
    mask = size - 1;
    keys = calloc(size, sizeof(Natural *));
    vals = calloc(size, sizeof(int));
    if (!keys || !vals)
        MemExhausted();

    /* Baby steps: ibase**j for j < m, stopping early at a short count */
    v = one_natural;
    for (j = 0; j < m && !found && !aborting; j++) {
        if (j > 0 && NaturalEqual(v, one_natural)) {
            found = j;
            break;
        }
        for (h = NaturalHash(v) & mask; keys[h]; h = (h + 1) & mask)
            ;
        keys[h] = v;
        vals[h] = j;
        v = NaturalTimesMod(v, nbase, nden);
    }

    /* Giant steps: ibase**(i*m), which v now holds for i = 1 */
    step = v;
    for (i = 1; !found && !aborting && (ptrdiff_t)(i - 1) * m < bound; i++) {
        for (h = NaturalHash(v) & mask; keys[h]; h = (h + 1) & mask) {
            if (NaturalEqual(keys[h], v)) {
                found = i * m - vals[h];
                break;
            }
        }
        v = NaturalTimesMod(v, step, nden);
    }
    free(keys);
    free(vals);
    if (!found || found > bound) {
        if (limit > 0)
            found = -1;
        else
            found = MAX_SENSIBLE + 1;
    }
    EXIT();
    return found;
}

static void
CheckDecimalLength(int prec, Natural *nden, int ibase, ptrdiff_t *initial, ptrdiff_t *repeat)
{
    ENTER();
    Natural *rem;
    Natural *nbase;
    Natural *g;
    int      offset;
    int      rep;

    nbase = NewNatural((double_digit)ibase);
    offset = 0;
    while (!NaturalEqual((g = NaturalGcd(nden, nbase)), one_natural)) {
        if (aborting) {
            EXIT();
            return;
        }
        offset++;
        if (prec >= 0 && offset > prec)
            break;
        nden = NaturalDivide(nden, g, &rem);
    }
    if (prec >= 0 && offset >= prec) {
        if (offset > prec)
            offset = -prec;
        else
            offset = prec;
        rep = 0;
    } else if (NaturalEqual(nden, one_natural)) {
        rep = 0;
    } else {
        if (prec >= 0)
            prec -= offset;
        rep = RationalRepeatLength(prec, nden, ibase);
    }
    *initial = offset;
    *repeat = rep;
    EXIT();
}

/*
 * Return 'rv' rounded to 'digits' places after the point in base
 * 'dig', a tie going to an even last digit, as NewFloat rounds
 */
static Value
RationalRoundDigits(Value rv, Natural *dig, ptrdiff_t digits)
{
    ENTER();
    Rational *r = &rv->rational;
    Natural  *scale = NaturalIntPow(dig, digits);
    Natural  *rem, *last;
    Natural  *q = NaturalDivide(NaturalTimes(r->num, scale), r->den, &rem);
    Natural  *twice = NaturalTimes(rem, two_natural);
    Bool      up = NaturalLess(r->den, twice);

    if (NaturalEqual(twice, r->den)) {
        (void)NaturalDivide(q, dig, &last);
        up = !NaturalEven(last);
    }
    if (up)
        q = NaturalPlus(q, one_natural);
    RETURN(NewRational(r->sign, q, scale));
}

/*
 * Write the first 'count' digits in 'base' of the fraction
 * partial / den, which is below one, to 'buf' with a terminating
 * NUL, returning the remainder. Long division a chunk of digits at a
 * time, as many as fit in a natural digit, keeps this linear in
 * 'count'; multiplying by base**count and converting the quotient at
 * once took time quadratic in it
 */
static Natural *
RationalFractionDigits(char *buf, ptrdiff_t count, Natural *partial, Natural *den, int base)
{
    ENTER();
    double_digit scale = (double_digit)base;
    int          chunk = 1;
    Natural     *nscale;
    StackPointer mark;
    ptrdiff_t    done = 0;

    while (scale <= MAXDIGIT / (double_digit)base) {
        scale *= (double_digit)base;
        chunk++;
    }
    nscale = NewNatural(scale);
    mark = MemMark();
    while (done < count && !aborting) {
        ptrdiff_t    n = chunk;
        Natural     *mult = nscale;
        double_digit q;

        if (count - done < n) {
            n = count - done;
            mult = NaturalIntPow(NewNatural((double_digit)base), n);
        }
        q = NaturalToDoubleDigit(NaturalDivide(NaturalTimes(partial, mult), den, &partial));
        for (ptrdiff_t i = n - 1; i >= 0; i--) {
            int d = (int)(q % (double_digit)base);

            if (d < 10)
                buf[done + i] = (char)('0' + d);
            else
                buf[done + i] = (char)('a' + d - 10);
            q /= (double_digit)base;
        }
        done += n;
        /* Keep only the remainder, so each chunk's temporaries go */
        partial = MemReturn(mark, partial);
    }
    buf[count] = '\0';
    RETURN(partial);
}

static Bool
RationalDecimalPrint(Value f, Value rv, uint32_t format, int base, int width, int prec,
                     uint32_t fill)
{
    ENTER();
    Rational *r = &rv->rational;
    Natural  *quo;
    Natural  *partial;
    Natural  *dig;
    ptrdiff_t exponent = 0;
    ptrdiff_t exponent_width = 0;
    Bool      scientific = False;
    char     *initial = 0;
    char     *repeat = 0;
    char     *whole;
    ptrdiff_t initial_width = 0, repeat_width = 0;
    ptrdiff_t frac_width;
    ptrdiff_t rep_width, brace_width = 0, dot_width = 0;
    ptrdiff_t whole_width;
    ptrdiff_t fraction_width;
    ptrdiff_t print_width;
    int       min_prec;
    Bool      use_braces = True;

    min_prec = 0;
    if (format == 'f' || format == 'e' || format == 'g') {
        min_prec = prec;
        use_braces = False;
    }
    if (prec == DEFAULT_OUTPUT_PRECISION) {
        min_prec = 0;
        prec = 15;
    } else if (prec == INFINITE_OUTPUT_PRECISION) {
        prec = -1;
        /* Only braces can show every digit of a repeating fraction */
        use_braces = True;
    }
    dig = NewNatural((double_digit)base);
    /*
     * Scale to [1, base) for 'e' format, and for small numbers in 'g'
     * format, which uses 'e' format when they need few enough digits
     */
    if (!NaturalZero(r->num)
        && (format == 'e' || (format == 'g' && prec != 0 && NaturalLess(r->num, r->den)))) {
        Bool      small = NaturalLess(r->num, r->den);
        Natural  *big = small ? r->den : r->num;
        Natural  *little = small ? r->num : r->den;
        Natural  *mag, *next;

        /*
         * big / little exceeds 2**bits, so this estimate of the
         * exponent's magnitude is never too large
         */
        ptrdiff_t bits = NaturalWidth(big) - NaturalWidth(little) - 1;
        ptrdiff_t k = 0;

        if (bits > 0)
            k = (ptrdiff_t)((double)bits / log2((double)base));
        mag = NaturalIntPow(dig, k);
        if (small) {
            /* The least k with num base**k >= den */
            while (NaturalLess(NaturalTimes(mag, r->num), r->den)) {
                mag = NaturalTimes(mag, dig);
                k++;
            }
        } else {
            /* The greatest k with den base**k <= num */
            while (!NaturalLess(r->num, NaturalTimes(next = NaturalTimes(mag, dig), r->den))) {
                mag = next;
                k++;
            }
        }
        /* As C does, and as floats do, below base**-4 */
        if (format == 'g' && k > 4)
            format = 'e';
        if (format == 'e') {
            if (small) {
                rv = RationalTimes(rv, NewRational(Positive, mag, one_natural), True);
                exponent = -k;
            } else {
                rv = RationalTimes(rv, NewRational(Positive, one_natural, mag), True);
                exponent = k;
            }
            r = &rv->rational;

            /*
             * Round to 'prec' digits now, which may carry the
             * mantissa to base, as with 9.9996 to 3 digits
             */
            if (prec >= 0) {
                rv = RationalRoundDigits(rv, dig, prec);
                r = &rv->rational;
                if (!NaturalLess(r->num, NaturalTimes(dig, r->den))) {
                    rv = RationalTimes(rv, NewRational(Positive, one_natural, dig), True);
                    r = &rv->rational;
                    exponent++;
                }
            }
            scientific = True;
            /* The label, the sign of a negative exponent, and its decimal digits */
            exponent_width = 2;
            if (exponent < 0)
                exponent_width++;
            for (ptrdiff_t ev = exponent; ev <= -10 || ev >= 10; ev /= 10)
                exponent_width++;
        }
    }
    /* Zero has exponent 0 */
    if (format == 'e' && NaturalZero(r->num)) {
        scientific = True;
        exponent_width = 2;
    }
    CheckDecimalLength(prec, r->den, base, &initial_width, &repeat_width);
    if (aborting) {
        EXIT();
        return False;
    }
    if ((rep_width = repeat_width)) {
        /*
         * When using %f format, just fill the
         * result with digits
         */
        if (!use_braces && prec != -1) {
            initial_width = -prec;
            repeat_width = 0;
            rep_width = 0;
        } else {
            if (repeat_width < 0)
                rep_width = prec - initial_width;
        }
    }
    /* Precision 0 rounds to a whole number, printed with no point */
    if (!use_braces && prec == 0) {
        rv = RationalRoundDigits(rv, dig, 0);
        r = &rv->rational;
        initial_width = repeat_width = rep_width = 0;
    }
    /* A value with no fraction digits still gets the precision asked for, in zeros */
    if (!initial_width && !repeat_width && min_prec > 0)
        initial_width = min_prec;
    if (initial_width) {
        if (initial_width < 0) {
            initial_width = -initial_width;
            rv = RationalRoundDigits(rv, dig, initial_width);
            r = &rv->rational;
        } else {
            if (!repeat_width && initial_width < min_prec)
                initial_width = min_prec;
        }
        initial = malloc((size_t)initial_width + 1);
        if (!initial) {
            EXIT();
            return False;
        }
    }
    quo = NaturalDivide(r->num, r->den, &partial);
    whole = NaturalSprint(0, quo, base, &whole_width);
    if (!whole) {
        free(initial);
        EXIT();
        return False;
    }
    brace_width = 0;
    if (repeat_width) {
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
    if (width) {
        if (width < 0)
            fraction_width = -width;
        else
            fraction_width = width;
        fraction_width = fraction_width - (whole_width + exponent_width);
        if (fraction_width < 0)
            fraction_width = 0;
        if (prec >= 0 && fraction_width > prec + dot_width)
            fraction_width = prec + dot_width;
    } else if (prec >= 0)
        fraction_width = prec + dot_width;
    else
        fraction_width = -1;
    /*
     * Start paring down parts of the output to fit the desired size
     */
    while (fraction_width >= 0 && (frac_width = dot_width + initial_width + rep_width + brace_width)
           && frac_width > fraction_width) {
        if (rep_width) {
            if (brace_width > 1) {
                brace_width = 1;
                repeat_width = -repeat_width;
            }
            rep_width = fraction_width - (dot_width + initial_width + brace_width);
            if (rep_width < 0) {
                rep_width = 0;
            }
        } else if (brace_width)
            brace_width = 0;
        else if (initial_width) {
            initial_width = fraction_width - dot_width;
            if (initial_width < 0)
                initial_width = 0;
        } else
            dot_width = 0;
    }
    if (initial_width) {
        partial = RationalFractionDigits(initial, initial_width, partial, r->den, base);
        if (aborting) {
            free(initial);
            EXIT();
            return False;
        }
    }
    if (rep_width) {
        if (rep_width > MAX_SENSIBLE) {
            repeat_width = -1;
            rep_width = MAX_SENSIBLE;
        }
        /*
         * allocate the output buffer; keep trying until this works
         */
        while (!(repeat = malloc((size_t)rep_width + 1))) {
            repeat_width = -1;
            rep_width >>= 1;
        }
        partial = RationalFractionDigits(repeat, rep_width, partial, r->den, base);
        if (aborting) {
            free(initial);
            free(repeat);
            EXIT();
            return False;
        }
        if (use_braces) {
            rep_width++; /* open { */
            if (repeat_width > 0)
                rep_width++; /* close } */
        }
    }
    fraction_width = initial_width + rep_width;
    print_width = whole_width + fraction_width + exponent_width;
    /* The point appears only before fraction digits */
    if (fraction_width)
        print_width = print_width + 1;
    if (r->sign == Negative)
        print_width = print_width + 1;

    Bool sign = r->sign == Negative;

    /* A zero fill goes after the sign, as in C */
    if (sign && fill == '0' && width > print_width) {
        FileOutput(f, '-');
        sign = False;
    }
    while (width > print_width) {
        FileOutchar(f, fill);
        width--;
    }
    if (sign)
        FileOutput(f, '-');
    FilePuts(f, whole);
    if (fraction_width)
        FileOutput(f, '.');
    if (initial_width)
        FilePuts(f, initial);
    if (rep_width) {
        if (use_braces)
            FileOutput(f, '{');
        FilePuts(f, repeat);
        if (use_braces && repeat_width > 0)
            FileOutput(f, '}');
    }
    /* Paring the output to fit may have dropped either part */
    free(initial);
    free(repeat);
    if (scientific) {
        /* Above base 10, 'e' is a digit; use '@' as GMP and MPFR do */
        if (base > 10)
            FilePrintf(f, "@%z", exponent);
        else
            FilePrintf(f, "e%z", exponent);
    }
    while (-width > print_width) {
        FileOutchar(f, fill);
        width++;
    }
    EXIT();
    return True;
}

static Bool
RationalPrint(Value f, Value rv, uint32_t format, int base, int width, int prec, uint32_t fill)
{
    Rational *r = &rv->rational;
    char     *num, *num_base, *den, *den_base;
    ptrdiff_t num_width, den_width;
    ptrdiff_t print_width;
    Bool      ret = True;

    if (format == 'a')
        return PrintAsFloat(f, rv, base, width, fill);

    if (base == 0)
        base = 10;
    switch (format) {
    case 'v':
        num_width = NaturalEstimateLength(r->num, base);
        num_base = malloc((size_t)num_width);
        num = NaturalSprint(num_base + num_width, r->num, base, &num_width);
        if (!num) {
            free(num_base);
            ret = False;
            break;
        }
        den_width = NaturalEstimateLength(r->den, base);
        den_base = malloc((size_t)den_width);
        den = NaturalSprint(den_base + den_width, r->den, base, &den_width);
        if (!den) {
            free(num_base);
            free(den_base);
            ret = False;
            break;
        }
        print_width = 1 + num_width + 1 + den_width + 1;
        if (r->sign == Negative)
            print_width++;
        while (width > print_width) {
            FileOutchar(f, fill);
            width--;
        }
        FileOutput(f, '(');
        if (r->sign == Negative)
            FileOutput(f, '-');
        FilePuts(f, num);
        FileOutput(f, '/');
        FilePuts(f, den);
        FileOutput(f, ')');
        free(num_base);
        free(den_base);
        while (-width > print_width) {
            FileOutchar(f, fill);
            width++;
        }
        break;
    default:
        ret = RationalDecimalPrint(f, rv, format, base, width, prec, fill);
        break;
    }
    return ret;
}

static void
RationalMark(void *object)
{
    Rational *rational = object;

    MemReference(rational->num);
    MemReference(rational->den);
}

ValueRep RationalRep = {
    { RationalMark, 0, "RationalRep" }, /* base */
    rep_rational, /* tag */
    {
     /* binary */
        RationalPlus, RationalMinus,
     RationalTimes, RationalDivide,
     NumericDiv, RationalMod,
     RationalLess, RationalEqual,
     0, 0,
     },
    {
     /* unary */
        RationalNegate, RationalFloor,
     RationalCeil, },
    RationalPromote,
    RationalReduce,
    RationalPrint,
    0,
    RationalHash,
};

Value
NewRational(Sign sign, Natural *num, Natural *den)
{
    ENTER();
    Value    ret;
    Natural *g;
    Natural *rem;

    if (NaturalZero(num))
        den = one_natural;
    else {
        if (NaturalLength(den) != 1 || NaturalDigits(den)[0] != 1) {
            g = NaturalGcd(num, den);
            if (NaturalLength(g) != 1 || NaturalDigits(g)[0] != 1) {
                num = NaturalDivide(num, g, &rem);
                den = NaturalDivide(den, g, &rem);
            }
        }
    }
    ret = ALLOCATE(&RationalRep.data, sizeof(Rational));
    ret->rational.sign = sign;
    ret->rational.num = num;
    ret->rational.den = den;
    RETURN(ret);
}

Value
NewIntRational(int i)
{
    ENTER();
    if (i < 0)
        /* Negate in two steps so the most negative int doesn't overflow */
        RETURN(NewRational(Negative, NewNatural((double_digit) - (i + 1) + 1), one_natural));
    else
        RETURN(NewRational(Positive, NewNatural((double_digit)i), one_natural));
}

Value
NewIntegerRational(Integer *i)
{
    ENTER();
    RETURN(NewRational(IntegerSign((Value)i), IntegerMag((Value)i), one_natural));
}
