/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include <math.h>
#include "nickle.h"

static Fpart *zero_fpart, *one_fpart;

#if 0
#define DebugV(s, v) FilePrintf(FileStdout, "%s %v\n", s, v)

#define DebugN(s, n) FilePrintf(FileStdout, "%s %n\n", s, n)

#define DebugFp(s, f)                                                                  \
    FilePrintf(FileStdout, "%s %s%n\n", s, (f)->sign == Negative ? "-" : "", (f)->mag)
#define DebugF(s, f)              \
    {                             \
        DebugFp(s, (f)->mant);    \
        DebugFp(" e ", (f)->exp); \
    }
#else
#define DebugV(s, v)
#define DebugN(s, n)
#define DebugFp(s, f)
#define DebugF(s, f)
#endif

#if 0
#define DPrintV(s, v) FilePrintf(FileStdout, "%s %v\n", s, v)

#define DPrintN(s, n) FilePrintf(FileStdout, "%s %n\n", s, n)

#define DPrintFp(s, f)                                                                 \
    FilePrintf(FileStdout, "%s %s%n\n", s, (f)->sign == Negative ? "-" : "", (f)->mag)
#define DPrintF(s, f)                                                                          \
    FilePrintf(FileStdout, "%s %s%n * 2**(%s%n)\n", s, (f)->mant->sign == Negative ? "-" : "", \
               (f)->mant->mag, (f)->exp->sign == Negative ? "-" : "", (f)->exp->mag)
#else
#define DPrintV(s, v)
#define DPrintN(s, n)
#define DPrintFp(s, f)
#define DPrintF(s, f)
#endif

static void
FpartMark(void *object)
{
    Fpart *f = object;
    MemReference(f->mag);
}

static DataType FpartType = { FpartMark, 0, "FpartType" };

static Fpart *
NewFpart(Sign sign, Natural *mag)
{
    ENTER();
    Fpart *ret;

    if (NaturalZero(mag))
        sign = Positive;
    ret = ALLOCATE(&FpartType, sizeof(Fpart));
    ret->sign = sign;
    ret->mag = mag;
    RETURN(ret);
}

static Fpart *
NewIntFpart(int i)
{
    ENTER();
    Sign         sign;
    double_digit mag;

    if (i < 0) {
        sign = Negative;
        /* Negate in two steps so the most negative int doesn't overflow */
        mag = (double_digit) - (i + 1) + 1;
    } else {
        sign = Positive;
        mag = (double_digit)i;
    }
    RETURN(NewFpart(sign, NewNatural(mag)));
}

static Fpart *
NewValueFpart(Value v)
{
    ENTER();
    Fpart *ret;
    switch (ValueTag(v)) {
    case rep_int:
        ret = NewIntFpart(ValueInt(v));
        break;
    case rep_integer:
        ret = NewFpart(IntegerSign(v), IntegerMag(v));
        break;
    default:
        ret = zero_fpart;
        break;
    }
    RETURN(ret);
}

static Fpart *
NewPtrdiffFpart(ptrdiff_t i)
{
    ENTER();
    RETURN(NewValueFpart(NewPtrdiffInteger(i)));
}

static Fpart *
FpartAdd(Fpart *a, Fpart *b, Bool negate)
{
    ENTER();
    Fpart *ret;

    switch (catagorize_signs(a->sign, negate ? SignNegate(b->sign) : b->sign)) {
    default:
    case BothPositive:
        ret = NewFpart(Positive, NaturalPlus(a->mag, b->mag));
        break;
    case FirstPositive:
        if (NaturalLess(a->mag, b->mag))
            ret = NewFpart(Negative, NaturalMinus(b->mag, a->mag));
        else
            ret = NewFpart(Positive, NaturalMinus(a->mag, b->mag));
        break;
    case SecondPositive:
        if (NaturalLess(a->mag, b->mag))
            ret = NewFpart(Positive, NaturalMinus(b->mag, a->mag));
        else
            ret = NewFpart(Negative, NaturalMinus(a->mag, b->mag));
        break;
    case BothNegative:
        ret = NewFpart(Negative, NaturalPlus(a->mag, b->mag));
        break;
    }
    RETURN(ret);
}

static Fpart *
FpartMult(Fpart *a, Fpart *b)
{
    ENTER();
    Sign sign;

    sign = Positive;
    if (a->sign != b->sign)
        sign = Negative;
    RETURN(NewFpart(sign, NaturalTimes(a->mag, b->mag)));
}

static Fpart *
FpartDivide(Fpart *a, Fpart *b, Natural **remp)
{
    ENTER();
    Natural *quo;
    Sign     sign;
    Fpart   *fquo;
    Natural *rem;

    sign = Positive;
    if (a->sign != b->sign)
        sign = Negative;

    quo = NaturalDivide(a->mag, b->mag, &rem);
    fquo = NewFpart(sign, quo);
    EXIT();
    REFERENCE(fquo);
    REFERENCE(rem);
    *remp = rem;
    return fquo;
}

static Fpart *
FpartRsl(Fpart *a, ptrdiff_t shift)
{
    ENTER();
    RETURN(NewFpart(a->sign, NaturalRsl(a->mag, shift)));
}

static Fpart *
FpartLsl(Fpart *a, ptrdiff_t shift)
{
    ENTER();
    RETURN(NewFpart(a->sign, NaturalLsl(a->mag, shift)));
}

static int
FpartRound(Fpart *a, ptrdiff_t shift)
{

    if (!shift)
        return 0;

    Natural  *n = a->mag;
    ptrdiff_t l = n->length;
    int       round = 0;
    digit    *d = NaturalDigits(n);

    while (l && shift > DIGITBITS) {
        if (*d++ != 0)
            round = 1;
        l--;
        shift -= DIGITBITS;
    }

    if (l) {
        digit last = *d;

        shift -= 2;
        if (shift < 0) {
            last <<= 1;
            shift = 0;
        } else {
            if (last & (((digit)1 << shift) - 1))
                round |= 1;
        }
        round |= (int)((last >> shift) & 3);
    }
    return round;
}

static Bool
FpartLess(Fpart *a, Fpart *b)
{
    switch (catagorize_signs(a->sign, b->sign)) {
    default:
    case BothPositive:
        return NaturalLess(a->mag, b->mag);
    case FirstPositive:
        return False;
    case SecondPositive:
        return True;
    case BothNegative:
        return NaturalLess(b->mag, a->mag);
    }
}

static Bool
FpartEqual(Fpart *a, Fpart *b)
{
    switch (catagorize_signs(a->sign, b->sign)) {
    default:
    case BothPositive:
    case BothNegative:
        return NaturalEqual(a->mag, b->mag);
    case FirstPositive:
        return False;
    case SecondPositive:
        return False;
    }
}

static Bool
FpartZero(Fpart *a)
{
    return NaturalZero(a->mag);
}

ptrdiff_t
FpartLength(Fpart *a)
{
    ptrdiff_t bits;
    digit     top;

    if (a->mag->length == 0)
        return 0;

    bits = (a->mag->length - 1) * LBASE2;
    top = NaturalDigits(a->mag)[a->mag->length - 1];
    while (top) {
        bits++;
        top >>= 1;
    }
    return bits;
}

static ptrdiff_t
FpartZeros(Fpart *a)
{
    ptrdiff_t i;
    ptrdiff_t zeros = 0;
    digit     top;

    if (a->mag->length == 0)
        return 0;
    for (i = 0; i < a->mag->length - 1; i++) {
        if (NaturalDigits(a->mag)[i] != 0)
            break;
        zeros += LBASE2;
    }
    top = NaturalDigits(a->mag)[i];
    while ((top & 1) == 0) {
        zeros++;
        top >>= 1;
    }
    return zeros;
}

/*
 * Store 'a' in '*p' and return True when it fits a ptrdiff_t
 */
static Bool
FpartToPtrdiff(Fpart *a, ptrdiff_t *p)
{
    ENTER();
    Bool ret = ValueToPtrdiff(NewInteger(a->sign, a->mag), p);
    EXIT();
    return ret;
}

static Fpart *
FpartNegate(Fpart *a)
{
    ENTER();
    RETURN(NewFpart(SignNegate(a->sign), a->mag));
}

int
FpartInit(void)
{
    ENTER();
    zero_fpart = NewFpart(Positive, zero_natural);
    MemAddRoot(zero_fpart);
    one_fpart = NewFpart(Positive, one_natural);
    MemAddRoot(one_fpart);
    EXIT();
    return 1;
}

static Value FloatNegate(Value av, int expandOk);

static Value
FloatAdd(Value av, Value bv, int expandOk, Bool negate)
{
    ENTER();
    Value     ret;
    Float    *a = &av->floats, *b = &bv->floats;
    Fpart    *dist;
    Fpart    *amant, *bmant, *mant;
    Fpart    *exp;
    ptrdiff_t d;
    ptrdiff_t prec;
    ptrdiff_t alen, blen;

    if (FpartZero(a->mant)) {
        if (negate)
            bv = FloatNegate(bv, expandOk);
        return bv;
    } else if (FpartZero(b->mant)) {
        return av;
    }
    dist = FpartAdd(a->exp, b->exp, True);
    ret = 0;
    if (NaturalLess(dist->mag, max_int_natural)) {
        d = NaturalToInt(dist->mag);
        if (dist->sign == Negative)
            d = -d;

        amant = a->mant;
        bmant = b->mant;
        alen = FpartLength(amant);
        blen = FpartLength(bmant);
        prec = 0;
        exp = 0;
        if (d >= 0) {
            if (alen + d <= blen + a->prec) {
                amant = FpartLsl(amant, d);
                exp = b->exp;
                prec = b->prec;
                if (a->prec + d < prec)
                    prec = a->prec + d;
            }
        } else {
            d = -d;
            if (blen + d <= alen + b->prec) {
                bmant = FpartLsl(bmant, d);
                exp = a->exp;
                prec = a->prec;
                if (b->prec + d < prec)
                    prec = b->prec + d;
            }
        }
        if (prec) {
            mant = FpartAdd(amant, bmant, negate);
            ret = NewFloat(mant, exp, prec);
        }
    }
    if (!ret) {
        if (dist->sign == Negative) {
            ret = bv;
            if (negate)
                ret = NewFloat(NewFpart(SignNegate(bv->floats.mant->sign), bv->floats.mant->mag),
                               bv->floats.exp, bv->floats.prec);
        } else
            ret = av;
    }
    RETURN(ret);
}

static Value
FloatPlus(Value av, Value bv, int expandOk)
{
    return FloatAdd(av, bv, expandOk, False);
}

static Value
FloatMinus(Value av, Value bv, int expandOk)
{
    return FloatAdd(av, bv, expandOk, True);
}

static Value
FloatTimes(Value av, Value bv, int expandOk)
{
    ENTER();
    Float    *a = &av->floats, *b = &bv->floats;
    Fpart    *mant;
    Fpart    *exp;
    ptrdiff_t prec;

    (void)expandOk;
    mant = FpartMult(a->mant, b->mant);
    exp = FpartAdd(a->exp, b->exp, False);
    if (a->prec < b->prec)
        prec = a->prec;
    else
        prec = b->prec;
    RETURN(NewFloat(mant, exp, prec));
}

static Value
FloatDivide(Value av, Value bv, int expandOk)
{
    ENTER();
    Float    *a = &av->floats, *b = &bv->floats;
    Fpart    *mant;
    Fpart    *amant = a->mant, *bmant = b->mant;
    Fpart    *exp;
    ptrdiff_t prec;
    ptrdiff_t iprec, alen;
    Natural  *rem;

    (void)expandOk;
    if (FpartZero(b->mant)) {
        RaiseStandardException(exception_divide_by_zero, 2, av, bv);
        RETURN(Void);
    }
    DebugF("Dividend ", a);
    DebugF("Divisor ", b);
    if (a->prec < b->prec)
        prec = a->prec;
    else
        prec = b->prec;
    exp = b->exp;
    /* 2 guard bits */
    iprec = prec + FpartLength(bmant) + 2;
    alen = FpartLength(amant);
    if (alen < iprec) {
        amant = FpartLsl(amant, iprec - alen);
        exp = FpartAdd(NewPtrdiffFpart(iprec - alen), exp, False);
    }
    exp = FpartAdd(a->exp, exp, True);
    DebugFp("amant ", amant);
    DebugFp("bmant ", bmant);
    mant = FpartDivide(amant, bmant, &rem);
    /* set the low guard bit if there was a remainder */
    if (!NaturalZero(rem))
        NaturalDigits(mant->mag)[0] |= 1;
    DebugFp("mant ", mant);
    DebugFp("exp ", exp);
    RETURN(NewFloat(mant, exp, prec));
}

static Value
FloatLess(Value av, Value bv, int expandOk)
{
    ENTER();
    Value  ret;
    Float *a = &av->floats, *b = &bv->floats;

    DPrintF("less a", a);
    DPrintF("less b", b);
    if (FpartEqual(a->mant, zero_fpart)) {
        if (b->mant->sign == Positive && !FpartEqual(b->mant, zero_fpart))
            ret = TrueVal;
        else
            ret = FalseVal;
    } else if (FpartEqual(b->mant, zero_fpart)) {
        if (a->mant->sign == Negative)
            ret = TrueVal;
        else
            ret = FalseVal;
    } else if (FpartEqual(a->exp, b->exp)) {
        ret = FalseVal;
        if (FpartLess(a->mant, b->mant))
            ret = TrueVal;
    } else {
        av = FloatMinus(av, bv, expandOk);
        DPrintF("less -", &av->floats);
        ret = FalseVal;
        if (av->floats.mant->sign == Negative)
            ret = TrueVal;
    }
    DPrintV("less:", ret);
    RETURN(ret);
}

static Value
FloatEqual(Value av, Value bv, int expandOk)
{
    ENTER();
    Value  ret;
    Float *a = &av->floats, *b = &bv->floats;

    if (FpartEqual(a->exp, b->exp)) {
        ret = FalseVal;
        if (FpartEqual(a->mant, b->mant))
            ret = TrueVal;
    } else {
        av = FloatMinus(av, bv, expandOk);
        ret = FalseVal;
        if (NaturalZero(av->floats.mant->mag))
            ret = TrueVal;
    }
    RETURN(ret);
}

static Value
FloatNegate(Value av, int expandOk)
{
    ENTER();
    Float *a = &av->floats;

    (void)expandOk;
    RETURN(NewFloat(FpartNegate(a->mant), a->exp, a->prec));
}

static Value
FloatInteger(Value av)
{
    ENTER();
    Float    *a = &av->floats;
    Natural  *mag;
    ptrdiff_t dist;

    if (a->exp->sign == Positive) {
        if (!NaturalBitsOk(a->exp->mag) || !FpartToPtrdiff(a->exp, &dist))
            RETURN(Void);
        mag = a->mant->mag;
        if (dist)
            mag = NaturalLsl(mag, dist);
        av = Reduce(NewInteger(a->mant->sign, mag));
    } else {
        RaiseStandardException(exception_invalid_unop_value, 2,
                               NewStrString("real is not an integer"), av);
    }
    RETURN(av);
}

static Value
FloatFloor(Value av, int expandOk)
{
    ENTER();
    Float    *a = &av->floats;
    Fpart    *mant;
    Fpart    *exp;
    ptrdiff_t d;

    (void)expandOk;
    if (a->exp->sign == Positive)
        RETURN(FloatInteger(av));
    if (!FpartToPtrdiff(a->exp, &d) || -d > a->prec) {
        if (a->mant->sign == Negative)
            RETURN(NewInt(-1));
        RETURN(Zero);
    }
    d = -d;
    mant = FpartRsl(a->mant, d);
    if (d && a->mant->sign == Negative) {
        mant = FpartAdd(mant, one_fpart, True);
        d--;
    }
    exp = zero_fpart;
    RETURN(FloatInteger(NewFloat(mant, exp, a->prec - d)));
}

static Value
FloatCeil(Value av, int expandOk)
{
    ENTER();
    Float    *a = &av->floats;
    Fpart    *mant;
    Fpart    *exp;
    ptrdiff_t d;

    (void)expandOk;
    if (a->exp->sign == Positive)
        RETURN(FloatInteger(av));
    if (!FpartToPtrdiff(a->exp, &d) || -d > a->prec) {
        if (a->mant->sign == Positive && !NaturalZero(a->mant->mag))
            RETURN(One);
        RETURN(Zero);
    }
    d = -d;
    mant = FpartRsl(a->mant, d);
    if (d && a->mant->sign == Positive) {
        mant = FpartAdd(mant, one_fpart, False);
        d--;
    }
    exp = zero_fpart;
    RETURN(FloatInteger(NewFloat(mant, exp, a->prec - d)));
}

static Value
FloatPromote(Value av, Value bv)
{
    ENTER();
    ptrdiff_t prec;

    if (!ValueIsFloat(av) && !ValueIsComplex(av)) {
        if (bv && ValueIsFloat(bv))
            prec = bv->floats.prec;
        else
            prec = FloatDefaultPrecision();
        av = NewValueFloat(av, prec);
    }
    RETURN(av);
}

static Value
FloatReduce(Value av)
{
    return av;
}

/*
 *  1/2 <= value / 2^exp2 < 1
 *  1/base <= value / base^expbase < 1
 *
 *  2^(exp2-1) <= value < 2^exp2
 *
 *  assign value = 2^(exp2-1)
 *
 *  then
 *
 *	1/base <= 2^(exp2-1) / base^expbase < 1
 *
 *	1 <= 2^(exp2-1) / (base^(expbase-1)) < base
 *
 *	-log(base) <= (exp2-1) * log(2) - expbase * log(base) < 1
 *
 *  ignoring the right inequality
 *
 *	0 <= (exp2 - 1) * log(2) - (expbase-1) * log(base)
 *	(expbase - 1) * log(base) <= (exp2 - 1) * log(2)
 *	(expbase - 1) <= (exp2 - 1) * log(2) / log(base);
 *	expbase <= (exp2 - 1) * log(2) / log(base) + 1;
 *	expbase = floor ((exp2 - 1) * log(2) / log(base) + 1);
 *
 *  Depending on value, expbase may need an additional digit
 */

#if 0
static Bool
NaturalBitSet (Natural *n, int i)
{
    int	d = i / LBASE2;
    int	b = i & LBASE2;

    return d < NaturalLength (n) && (NaturalDigits(n)[d] & 1 << b);
}
#endif

static Value
FloatExp(Value exp2, Value *ratio, int ibase, ptrdiff_t prec)
{
    ENTER();
    double dscale;
    Value  scale;
    Value  r;
    Value  min, max, mean, nmean;
    Value  pow2;
    Value  base_f;
    Value  two;
    Value  two_f;
    Bool   done;

    DebugV("exp2", exp2);
    two = NewInt(2);
    two_f = NewIntFloat(2, prec + 32);
    base_f = NewIntFloat(ibase, prec + 32);
    /*
     * Compute expbase, this is a bit tricky as log is only
     * available in floats
     */
    dscale = log(2) / log(ibase) * MAX_NICKLE_INT;
    scale = Divide(NewInt((int)dscale), NewInt(MAX_NICKLE_INT));
    /*
     * min = floor (((exp2 - 1) * scale) + 1);
     */
    min = Floor(Plus(Times(Minus(exp2, One), scale), One));
    if (Negativep(min))
        max = Div(min, two);
    else
        max = Times(min, two);

    /*
     * pow2 = 2 ** (exp2-1)
     */
    pow2 = Pow(two_f, Minus(exp2, One));

    mean = 0;
    done = False;
    do {
        if (aborting) {
            EXIT();
            *ratio = Void;
            return Void;
        }
        nmean = Div(Plus(min, max), two);
        if (mean && True(Equal(nmean, mean))) {
            nmean = Plus(nmean, One);
            done = True;
        }
        mean = nmean;
        DebugV("min ", min);
        DebugV("mean", mean);
        DebugV("max ", max);
        /*
         * r = 2 ** (exp2-1) / (base ** (mean - 1))
         */
        r = Divide(pow2, Pow(base_f, Minus(mean, One)));
        if (done)
            break;
        if (True(Less(One, r)))
            min = mean;
        else
            max = mean;
    } while (False(Equal(max, min)));
    mean = Minus(mean, One);
    /*    r = Divide (pow2, Pow (base, Minus (mean, One)));*/
    r = Divide(Pow(two_f, exp2), Pow(base_f, mean));
    /*    r = Divide (pow2, Pow (base, mean)); */
    EXIT();
    REFERENCE(mean);
    REFERENCE(r);
    *ratio = r;
    return mean;
}

static Bool
FloatPrint(Value f, Value fv, uint32_t format, int base, int width, int print_prec, uint32_t fill)
{
    ENTER();
    Float    *a = &fv->floats;
    Value     expbase;
    Fpart    *exp;
    Natural  *int_n;
    Natural  *frac_n;
    Value     ratio;
    Value     down;
    Value     fratio;
    Value     m;
    Value     int_part;
    Value     frac_part;
    ptrdiff_t length;
    ptrdiff_t prec = print_prec;
    int       orig_prec = print_prec;
    ptrdiff_t mant_prec;
    ptrdiff_t frac_prec;
    ptrdiff_t dig_max;
    ptrdiff_t exp_value;
    ptrdiff_t exp_width;
    ptrdiff_t int_width;
    ptrdiff_t frac_width;
    ptrdiff_t print_width;
    Bool      negative;
    char     *int_buffer;
    char     *int_string;
    char     *frac_buffer;
    char     *frac_string;
    char     *exp_string = 0;
    Bool      rounded = False;
    int       base_exp;
    char     *exp_label = "e";
    Bool      leader = False;
    int       exp_scale;

    DPrintF("print", a);
    length = FpartLength(a->mant);

    if (format == 'a') {
        base = 16;
        base_exp = 10;
        exp_scale = 2;
        leader = True;
        exp_label = "p";
        if (prec == DEFAULT_OUTPUT_PRECISION)
            prec = length * DIGITBITS / 4;
    } else {

        if (base <= 0) {
            base = 10;
        }

        if (prec == DEFAULT_OUTPUT_PRECISION)
            prec = 15;
        base_exp = base;
        exp_scale = base;
    }

    mant_prec = (ptrdiff_t)ceil((double)a->prec * log(2) / log(base)) + 1;

    expbase = FloatExp(Plus(NewPtrdiffInteger(length), NewInteger(a->exp->sign, a->exp->mag)),
                       &ratio, exp_scale, a->prec);
    if (aborting) {
        EXIT();
        return False;
    }
    DPrintV("expbase", expbase);
    DPrintF("ratio  ", &ratio->floats);
    down = Pow(NewInt(2), NewPtrdiffInteger(length));
    DPrintV("down   ", down);
    fratio = Divide(ratio, down);
    DPrintF("fratio ", &fratio->floats);
    negative = a->mant->sign == Negative;
    m = NewInteger(Positive, a->mant->mag);

    m = Times(m, fratio);

try_again_e:
    DPrintF("scaled m", &m->floats);
    if (True(Less(m, One))) {
        m = Times(m, NewInt(base));
        expbase = Minus(expbase, One);
        DPrintF("make m >= 1", &m->floats);
        DPrintV("expbase", expbase);
    } else if (False(Less(m, NewInt(base)))) {
        m = Divide(m, NewInt(base));
        expbase = Plus(expbase, One);
        DPrintF("make m < base", &m->floats);
        DPrintV("expbase", expbase);
    }
    exp = NewValueFpart(expbase);
    switch (format) {
    case 'a':
    case 'e':
    case 'E':
    case 'f':
        break;
    default:
        dig_max = prec;
        if ((exp->sign == Positive && dig_max >= 0
             && (!FpartToPtrdiff(exp, &exp_value) || exp_value >= dig_max))
            || (exp->sign == Negative && NaturalLess(NewNatural(4), exp->mag))) {
            format = 'e';
        } else {
            format = 'f';
        }
    }

    if (format == 'f') {
        /*
         * Use the exact value, mant * 2**exp, so that every integer
         * digit is correct and not just those within the precision
         */
        m = Times(NewInteger(Positive, a->mant->mag),
                  Pow(NewInt(2), NewInteger(a->exp->sign, a->exp->mag)));
        DPrintF("f format m", &m->floats);
        exp_width = 0;
        if (prec == INFINITE_OUTPUT_PRECISION) {
            prec = mant_prec;
            if (ValueIsInt(expbase)) {
                if (ValueInt(expbase) < 0)
                    prec -= ValueInt(expbase);
                else if (ValueInt(expbase) > prec)
                    prec = ValueInt(expbase);
            }
        }
    } else {
        exp_string = NaturalSprint(0, exp->mag, base_exp, &exp_width);
        if (aborting) {
            EXIT();
            return True;
        }
        exp_width++;
        if (exp->sign == Negative)
            exp_width++;
        if (prec == INFINITE_OUTPUT_PRECISION)
            prec = mant_prec;
    }

try_again_f:
    int_part = Floor(m);
    frac_part = Minus(m, int_part);
    DPrintV("m int", int_part);
    DPrintF("m frac", &frac_part->floats);

    if (ValueIsInteger(int_part))
        int_n = IntegerMag(int_part);
    else
        int_n = NewNatural((double_digit)ValueInt(int_part));

    int_width = NaturalEstimateLength(int_n, base);
    if (leader)
        int_width += 2;
    if (negative)
        int_width++;

    int_buffer = malloc((size_t)int_width + 1);
    int_string = NaturalSprint(int_buffer + int_width + 1, int_n, base, &int_width);

    if (aborting) {
        EXIT();
        return True;
    }
    frac_prec = mant_prec - int_width;
    if (*int_string == '0')
        frac_prec++;

    if (leader) {
        *--int_string = 'x';
        *--int_string = '0';
        int_width += 2;
    }
    if (negative) {
        *--int_string = '-';
        int_width++;
    }

    if (width) {
        if (width > 0)
            frac_width = width - int_width - exp_width;
        else
            frac_width = -width - int_width - exp_width;
        if (prec > 0)
            if (frac_width > prec + 1)
                frac_width = prec + 1;
    } else {
        if (prec == INFINITE_OUTPUT_PRECISION)
            frac_width = frac_prec + 1;
        else
            frac_width = prec + 1;
    }

    /*
     * Limit fraction to available precision
     */
    if (frac_width > frac_prec + 1)
        frac_width = frac_prec + 1;

    if (frac_width < 2)
        frac_width = 0;
    /*
     * Round the fractional part up by 1/2 beyond the
     * last digit to be printed.
     */
    if (!rounded) {
        ptrdiff_t frac_digits = frac_width == 0 ? 0 : frac_width - 1;
        Value     round
            = Times(Divide(One, NewInt(2)), Pow(NewInt(base), NewPtrdiffInteger(-frac_digits)));
        frac_part = Plus(frac_part, round);

        /*
         * If the fractional overflowed, bump the integer part
         * and try again
         */
        if (GreaterEqual(frac_part, One) == TrueVal) {
            rounded = True;
            free(int_buffer);
            m = Plus(int_part, frac_part);
            if (format == 'e')
                goto try_again_e;
            else
                goto try_again_f;
        }
    }
    frac_buffer = 0;
    frac_string = 0;
    if (frac_width)
        frac_part = Floor(Times(frac_part, Pow(NewInt(base), NewPtrdiffInteger(frac_width - 1))));
    if (frac_width && (!Zerop(frac_part) || orig_prec > 0)) {
        ptrdiff_t frac_wrote;

        if (ValueIsInteger(frac_part))
            frac_n = IntegerMag(frac_part);
        else
            frac_n = NewNatural((double_digit)ValueInt(frac_part));

        frac_buffer = malloc((size_t)frac_width + 1);
        frac_string = NaturalSprint(frac_buffer + frac_width + 1, frac_n, base, &frac_wrote);
        if (aborting) {
            EXIT();
            return True;
        }

        while (frac_wrote < frac_width - 1) {
            *--frac_string = '0';
            frac_wrote++;
        }
        *--frac_string = '.';
        if (orig_prec < 0)
            while (frac_buffer[frac_width - 1] == '0')
                frac_buffer[--frac_width] = '\0';
    } else
        frac_width = 0;

    print_width = int_width + frac_width + exp_width;
    while (width > print_width) {
        FileOutchar(f, fill);
        width--;
    }

    FilePuts(f, int_string);
    if (frac_string)
        FilePuts(f, frac_string);

    if (exp_width) {
        FilePuts(f, exp_label);
        if (exp->sign == Negative)
            FilePuts(f, "-");
        FilePuts(f, exp_string);
    }
    while (-width > print_width) {
        FileOutchar(f, fill);
        width++;
    }
    free(int_buffer);
    if (frac_buffer)
        free(frac_buffer);

    EXIT();
    return True;
}

static HashValue
FloatHash(Value av)
{
    Float *a = &av->floats;

    return (NaturalHash(a->mant->mag) ^ a->mant->sign ^ NaturalHash(a->exp->mag) ^ a->exp->sign);
}

static void
FloatMark(void *object)
{
    Float *f = object;

    MemReference(f->mant);
    MemReference(f->exp);
}

ValueRep FloatRep = {
    { FloatMark, 0, "FloatRep" }, /* base */
    rep_float, /* tag */
    {
     /* binary */
        FloatPlus, FloatMinus,
     FloatTimes, FloatDivide,
     NumericDiv, NumericMod,
     FloatLess, FloatEqual,
     0, 0,
     },
    {
     /* unary */
        FloatNegate, FloatFloor,
     FloatCeil, },
    FloatPromote,
    FloatReduce,
    FloatPrint,
    0,
    FloatHash,
};

Value
NewFloat(Fpart *mant, Fpart *exp, ptrdiff_t prec)
{
    ENTER();
    ptrdiff_t bits, dist;
    Value     ret;
    int       round;

    DebugFp("New mant", mant);
    DebugFp("New exp ", exp);
    /*
     * Trim to specified precision
     */
    bits = FpartLength(mant);
    if (bits > prec) {
        dist = bits - prec;
        exp = FpartAdd(exp, NewPtrdiffFpart(dist), False);
        round = FpartRound(mant, dist);
        mant = FpartRsl(mant, dist);
        if (round & 2) {
            if ((round & 1) || !NaturalEven(mant->mag)) {
                mant->mag = NaturalPlus(mant->mag, one_natural);
                if (FpartLength(mant) != prec) {
                    exp = FpartAdd(exp, one_fpart, False);
                    mant = FpartRsl(mant, 1);
                }
            }
        }
    }
    /*
     * Canonicalize by shifting to a 1 in the LSB
     */
    dist = FpartZeros(mant);
    if (dist) {
        exp = FpartAdd(exp, NewPtrdiffFpart(dist), False);
        mant = FpartRsl(mant, dist);
    }
    bits = FpartLength(mant);
    if (bits == 0)
        exp = mant = zero_fpart;
    DebugFp("Can mant", mant);
    DebugFp("Can exp ", exp);
    ret = ALLOCATE(&FloatRep.data, sizeof(Float));
    ret->floats.mant = mant;
    ret->floats.exp = exp;
    ret->floats.prec = prec;
    RETURN(ret);
}

Value
NewIntFloat(int i, ptrdiff_t prec)
{
    ENTER();
    RETURN(NewFloat(NewIntFpart(i), zero_fpart, prec));
}

Value
NewIntegerFloat(Integer *i, ptrdiff_t prec)
{
    ENTER();
    Fpart *mant;

    mant = NewFpart(IntegerSign((Value)i), IntegerMag((Value)i));
    RETURN(NewFloat(mant, zero_fpart, prec));
}

Value
NewNaturalFloat(Sign sign, Natural *n, ptrdiff_t prec)
{
    ENTER();
    Fpart *mant;

    mant = NewFpart(sign, n);
    RETURN(NewFloat(mant, zero_fpart, prec));
}

Value
NewRationalFloat(Rational *r, ptrdiff_t prec)
{
    ENTER();
    Fpart    *num = NewFpart(r->sign, r->num);
    Fpart    *den = NewFpart(Positive, r->den);
    Fpart    *mant;
    Fpart    *exp = zero_fpart;
    ptrdiff_t iprec, nlen;
    Natural  *rem;

    /*
     * Divide the exact numerator by the exact denominator, as
     * FloatDivide does, so the result is rounded only once
     */
    /* 2 guard bits */
    iprec = prec + FpartLength(den) + 2;
    nlen = FpartLength(num);
    if (nlen < iprec) {
        num = FpartLsl(num, iprec - nlen);
        exp = FpartAdd(exp, NewPtrdiffFpart(iprec - nlen), True);
    }
    mant = FpartDivide(num, den, &rem);
    /* set the low guard bit if there was a remainder */
    if (!NaturalZero(rem))
        NaturalDigits(mant->mag)[0] |= 1;
    RETURN(NewFloat(mant, exp, prec));
}

#define SCALE_BITS 52
#define SCALE      0x1p52 /* 2 ** 52 */

Value
NewDoubleFloat(double d)
{
    ENTER();
    int          e;
    double       m;
    Sign         ms;

    double_digit dd;
    if (d == 0.0)
        RETURN(Zero);
    e = ilogb(d);
#ifdef HAVE_SIGNIFICAND
    m = significand(d);
#else
    m = scalbn(d, -e);
#endif
    ms = Positive;
    if (m < 0) {
        ms = Negative;
        m = -m;
    }
    e = e - SCALE_BITS;
    dd = (double_digit)(m * SCALE + 0.5);
    RETURN(NewFloat(NewFpart(ms, NewNatural(dd)), NewIntFpart(e), SCALE_BITS));
}

Value
FloatLdexp(Value av, Value ev)
{
    ENTER();
    RETURN(NewFloat(av->floats.mant, FpartAdd(av->floats.exp, NewValueFpart(ev), False),
                    av->floats.prec));
}

/*
 * Store the float precision 'v' in '*precp', raising an exception and
 * returning False when it isn't one. Negative integers too large for a
 * ptrdiff_t read as 0, which callers reject or replace like any other
 * precision below 1.
 */
Bool
FloatPrecisionPart(Value v, ptrdiff_t *precp, char *error)
{
    ptrdiff_t prec;

    if (!ValueToPtrdiff(v, &prec)) {
        if (ValueIsInteger(v) && Negativep(v)) {
            *precp = 0;
            return True;
        }
        if (ValueIsInteger(v))
            RaiseStandardException(exception_out_of_memory, 1, NewStrString("precision too large"));
        else
            RaiseStandardException(exception_invalid_argument, 3, NewStrString(error), NewInt(0),
                                   v);
        return False;
    }
    if (prec > FLOAT_MAX_PREC) {
        RaiseStandardException(exception_out_of_memory, 1, NewStrString("precision too large"));
        return False;
    }
    *precp = prec;
    return True;
}

/*
 * The precision for new imprecise values: float_precision when it is
 * greater than 1, else DEFAULT_FLOAT_PREC
 */
ptrdiff_t
FloatDefaultPrecision(void)
{
    Value     float_prec = lookupVar(0, "float_precision");
    ptrdiff_t prec;

    if (float_prec && FloatPrecisionPart(float_prec, &prec, "invalid float_precision") && prec > 1)
        return prec;
    return DEFAULT_FLOAT_PREC;
}

Value
NewValueFloat(Value av, ptrdiff_t prec)
{
    ENTER();

    switch (ValueTag(av)) {
    case rep_int:
        av = NewIntFloat(ValueInt(av), prec);
        break;
    case rep_integer:
        av = NewIntegerFloat(&av->integer, prec);
        break;
    case rep_rational:
        av = NewRationalFloat(&av->rational, prec);
        break;
    case rep_float:
        if (prec == av->floats.prec)
            break;
        av = NewFloat(av->floats.mant, av->floats.exp, prec);
        break;
    default:
        break;
    }
    RETURN(av);
}

double
DoublePart(Value av, char *error)
{
    double    mantissa;
    ptrdiff_t i;
    int       e;
    digit    *mt;
    double    div;

    av = NewValueFloat(av, 64);
    if (!ValueIsFloat(av)) {
        RaiseStandardException(exception_invalid_argument, 3, NewStrString(error), NewInt(0), av);
        return 0.0;
    }
    if (NaturalLess(av->floats.exp->mag, max_int_natural))
        e = NaturalToInt(av->floats.exp->mag);
    else
        e = MAX_NICKLE_INT;
    if (e > 1023) {
        if (av->floats.exp->sign == Negative)
            return 0.0;

        RaiseStandardException(exception_invalid_argument, 3, NewStrString(error), NewInt(0), av);
        return 0.0;
    }
    if (av->floats.exp->sign == Negative)
        e = -e;

    mantissa = 0.0;
    i = av->floats.mant->mag->length;
    e += (int)(DIGITBITS * i);
    mt = NaturalDigits(av->floats.mant->mag) + i;
    div = 1.0 / (double)BASE;
    while (i--) {
        mantissa = mantissa + (double)*--mt * div;
        div *= 1.0 / (double)BASE;
    }
    if (av->floats.mant->sign == Negative)
        mantissa = -mantissa;
    return mantissa * pow(2.0, (double)e);
}
