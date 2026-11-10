/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * integer.c
 *
 * operations on integers
 */

#include "nickle.h"

#define IMag(i)  ((Natural *)((long)((i)->magn) & ~1))
#define ISign(i) ((Sign)((long)((i)->magn) & 1))

int
IntegerToInt(Integer *i)
{
    int result;

    result = NaturalToInt(IMag(i));
    if (ISign(i) == Negative)
        result = -result;
    return result;
}

signed_digit
IntegerToSignedDigit(Integer *i)
{
    double_digit dd;
    signed_digit sd;

    dd = NaturalToDoubleDigit(IMag(i));
    if (ISign(i) == Negative)
        sd = -(signed_digit)dd;
    else
        sd = (signed_digit)dd;
    return sd;
}

int
IntegerFitsSignedDigit(Integer *i)
{
    return NaturalLess(IMag(i), max_signed_digit_natural);
}

static Value
IntegerPlus(Value av, Value bv, int expandOk)
{
    ENTER();
    Integer *a = &av->integer, *b = &bv->integer;
    Value    ret;

    (void)expandOk;
    switch (catagorize_signs(ISign(a), ISign(b))) {
    case BothPositive:
    default:
        ret = NewInteger(Positive, NaturalPlus(IMag(a), IMag(b)));
        break;
    case FirstPositive:
        if (NaturalLess(IMag(a), IMag(b)))
            ret = NewInteger(Negative, NaturalMinus(IMag(b), IMag(a)));
        else
            ret = NewInteger(Positive, NaturalMinus(IMag(a), IMag(b)));
        break;
    case SecondPositive:
        if (NaturalLess(IMag(a), IMag(b)))
            ret = NewInteger(Positive, NaturalMinus(IMag(b), IMag(a)));
        else
            ret = NewInteger(Negative, NaturalMinus(IMag(a), IMag(b)));
        break;
    case BothNegative:
        ret = NewInteger(Negative, NaturalPlus(IMag(a), IMag(b)));
        break;
    }
    RETURN(ret);
}

static Value
IntegerMinus(Value av, Value bv, int expandOk)
{
    ENTER();
    Integer *a = &av->integer, *b = &bv->integer;
    Value    ret;

    (void)expandOk;
    switch (catagorize_signs(ISign(a), ISign(b))) {
    case BothPositive:
    default:
        if (NaturalLess(IMag(a), IMag(b)))
            ret = NewInteger(Negative, NaturalMinus(IMag(b), IMag(a)));
        else
            ret = NewInteger(Positive, NaturalMinus(IMag(a), IMag(b)));
        break;
    case FirstPositive:
        ret = NewInteger(Positive, NaturalPlus(IMag(a), IMag(b)));
        break;
    case SecondPositive:
        ret = NewInteger(Negative, NaturalPlus(IMag(a), IMag(b)));
        break;
    case BothNegative:
        if (NaturalLess(IMag(a), IMag(b)))
            ret = NewInteger(Positive, NaturalMinus(IMag(b), IMag(a)));
        else
            ret = NewInteger(Negative, NaturalMinus(IMag(a), IMag(b)));
        break;
    }
    RETURN(ret);
}

static Value
IntegerTimes(Value av, Value bv, int expandOk)
{
    ENTER();
    Integer *a = &av->integer, *b = &bv->integer;
    Sign     sign;

    (void)expandOk;
    sign = Positive;
    if (ISign(a) != ISign(b))
        sign = Negative;
    RETURN(NewInteger(sign, NaturalTimes(IMag(a), IMag(b))));
}

static Value
IntegerDivide(Value av, Value bv, int expandOk)
{
    ENTER();
    Integer *a = &av->integer, *b = &bv->integer;
    Natural *rem;
    Sign     sign;

    if (NaturalZero(IMag(b))) {
        RaiseStandardException(exception_divide_by_zero, 2, av, bv);
        RETURN(Void);
    }
    sign = Positive;
    if (ISign(a) != ISign(b))
        sign = Negative;
    if (expandOk)
        RETURN(NewRational(sign, IMag(a), IMag(b)));
    else
        RETURN(NewInteger(sign, NaturalDivide(IMag(a), IMag(b), &rem)));
}

static Value
IntegerDiv(Value av, Value bv, int expandOk)
{
    ENTER();
    Integer *a = &av->integer, *b = &bv->integer;
    Sign     sign;
    Natural *quo, *rem;

    (void)expandOk;
    if (NaturalZero(IMag(b))) {
        RaiseStandardException(exception_divide_by_zero, 2, av, bv);
        RETURN(Void);
    }
    quo = NaturalDivide(IMag(a), IMag(b), &rem);
    sign = Positive;
    if (ISign(a) != ISign(b))
        sign = Negative;
    if (ISign(a) == Negative && !NaturalZero(rem))
        quo = NaturalPlus(quo, one_natural);
    RETURN(NewInteger(sign, quo));
}

static Value
IntegerMod(Value av, Value bv, int expandOk)
{
    ENTER();
    Integer *a = &av->integer, *b = &bv->integer;
    Natural *rem;

    (void)expandOk;
    if (NaturalZero(IMag(b))) {
        RaiseStandardException(exception_divide_by_zero, 2, av, bv);
        RETURN(Void);
    }
    (void)NaturalDivide(IMag(a), IMag(b), &rem);
    if (ISign(a) == Negative && !NaturalZero(rem))
        rem = NaturalMinus(IMag(b), rem);
    RETURN(NewInteger(Positive, rem));
}

static Value
IntegerLess(Value av, Value bv, int expandOk)
{
    Integer *a = &av->integer, *b = &bv->integer;
    Value    ret;

    (void)expandOk;
    ret = FalseVal;
    switch (catagorize_signs(ISign(a), ISign(b))) {
    case BothPositive:
        if (NaturalLess(IMag(a), IMag(b)))
            ret = TrueVal;
        break;
    case FirstPositive:
        break;
    case SecondPositive:
        ret = TrueVal;
        break;
    case BothNegative:
        if (NaturalLess(IMag(b), IMag(a)))
            ret = TrueVal;
        break;
    }
    return ret;
}

static Value
IntegerEqual(Value av, Value bv, int expandOk)
{
    Integer *a = &av->integer, *b = &bv->integer;

    (void)expandOk;
    if (ISign(a) == ISign(b) && NaturalEqual(IMag(a), IMag(b)))
        return TrueVal;
    return FalseVal;
}

/*
 * Generate the two's complement digits of an integer from its
 * magnitude, one at a time, starting at the least significant.
 * Past the end of the magnitude, a non-negative value extends
 * with zeros and a negative value with ones.
 */
typedef struct _twosDigits {
    digit    *digits;
    ptrdiff_t length;
    digit     carry;  /* set while every lower digit was zero */
    digit     extend; /* 0 or ~0 */
} TwosDigits;

static void
TwosDigitsInit(TwosDigits *t, Integer *i)
{
    t->digits = NaturalDigits(IMag(i));
    t->length = NaturalLength(IMag(i));
    t->carry = ISign(i) == Negative;
    t->extend = ISign(i) == Negative ? ~(digit)0 : 0;
}

static digit
TwosDigitsNext(TwosDigits *t, ptrdiff_t i)
{
    digit d;

    if (i >= t->length)
        return t->extend;
    d = t->digits[i];
    if (t->extend) {
        d = ~d + t->carry;
        if (d)
            t->carry = 0;
    }
    return d;
}

/*
 * Bitwise operations on two's complement values. Digits past the
 * end of the shorter operand are all zeros or all ones, so the walk
 * stops once those force every remaining result digit to the
 * result's sign extension.
 */
static Value
IntegerBitwise(Value av, Value bv, BinaryOp op)
{
    ENTER();
    Integer   *a = &av->integer, *b = &bv->integer;
    Bool       aneg = ISign(a) == Negative, bneg = ISign(b) == Negative;
    ptrdiff_t  alen = NaturalLength(IMag(a)), blen = NaturalLength(IMag(b));
    ptrdiff_t  shorter = alen < blen ? alen : blen;
    ptrdiff_t  longer = alen < blen ? blen : alen;
    ptrdiff_t  len, i;
    Bool       neg;
    digit      extend, carry, x, y;
    digit     *r;
    Natural   *m;
    TwosDigits at, bt;

    if (op == LxorOp) {
        neg = aneg != bneg;
        len = longer;
    } else if (op == LandOp) {
        neg = aneg && bneg;
        if (aneg && bneg)
            len = longer;
        else if (aneg)
            len = blen;
        else if (bneg)
            len = alen;
        else
            len = shorter;
    } else {
        neg = aneg || bneg;
        if (aneg && bneg)
            len = shorter;
        else if (aneg)
            len = alen;
        else if (bneg)
            len = blen;
        else
            len = longer;
    }
    extend = neg ? ~(digit)0 : 0;

    /* One more digit for the carry when negating the result */
    m = AllocNatural(len + 1);
    r = NaturalDigits(m);
    TwosDigitsInit(&at, a);
    TwosDigitsInit(&bt, b);
    for (i = 0; i < len; i++) {
        x = TwosDigitsNext(&at, i);
        y = TwosDigitsNext(&bt, i);
        switch (op) {
        case LandOp:
            r[i] = x & y;
            break;
        case LorOp:
            r[i] = x | y;
            break;
        default:
            r[i] = x ^ y;
            break;
        }
    }

    /* Trim the sign extension */
    while (len > 0 && r[len - 1] == extend)
        len--;

    /* Negate a negative result in place to get its magnitude */
    if (neg) {
        carry = 1;
        for (i = 0; i < len; i++) {
            r[i] = ~r[i] + carry;
            if (r[i])
                carry = 0;
        }
        if (carry)
            r[len++] = carry;
    }
    m->length = len;
    RETURN(NewInteger(neg ? Negative : Positive, m));
}

static Value
IntegerLand(Value av, Value bv, int expandOk)
{
    (void)expandOk;
    return IntegerBitwise(av, bv, LandOp);
}

static Value
IntegerLor(Value av, Value bv, int expandOk)
{
    (void)expandOk;
    return IntegerBitwise(av, bv, LorOp);
}

static Value
IntegerLxor(Value av, Value bv, int expandOk)
{
    (void)expandOk;
    return IntegerBitwise(av, bv, LxorOp);
}

static Value
IntegerNegate(Value av, int expandOk)
{
    Integer *a = &av->integer;

    (void)expandOk;
    return NewInteger(SignNegate(ISign(a)), IMag(a));
}

static Value
IntegerFloor(Value av, int expandOk)
{
    (void)expandOk;
    return av;
}

static Value
IntegerCeil(Value av, int expandOk)
{
    (void)expandOk;
    return av;
}

static Value
IntegerPromote(Value av, Value bv)
{
    (void)bv;
    if (ValueIsInt(av))
        av = NewIntInteger(ValueInt(av));
    return av;
}

static Value
IntegerReduce(Value av)
{
    Integer *a = &av->integer;

    if (NaturalLess(IMag(a), max_int_natural))
        av = NewInt(IntegerToInt(a));
    return av;
}

static Bool
IntegerPrint(Value f, Value iv, char format, int base, int width, int prec, int fill)
{
    ENTER();
    Integer  *i = &iv->integer;
    char     *result;
    ptrdiff_t print_width;
    ptrdiff_t fraction_width;

    if (format == 'a') {
        Value     fv = NewIntegerFloat(i, DEFAULT_FLOAT_PREC);
        ValueRep *rep = ValueRep(fv);
        return (*rep->print)(f, fv, format, base, width, DEFAULT_OUTPUT_PRECISION, fill);
    }

    if (base == 0)
        base = 10;
    result = NaturalSprint(0, IMag(i), base, &print_width);
    if (result) {
        if (ISign(i) == Negative)
            print_width++;
        fraction_width = 0;
        if (prec >= 0) {
            int avail_width;

            if (width > 0)
                avail_width = width;
            else
                avail_width = -width;
            fraction_width = prec + 1;
            if (avail_width > 0) {
                if (print_width + fraction_width > avail_width) {
                    fraction_width = avail_width - print_width;
                    if (fraction_width < 0)
                        fraction_width = 0;
                }
            }
        }
        print_width += fraction_width;
        while (width > print_width) {
            FileOutchar(f, fill);
            width--;
        }
        if (ISign(i) == Negative)
            FileOutput(f, '-');
        FilePuts(f, result);
        if (fraction_width) {
            FileOutput(f, '.');
            --fraction_width;
            while (fraction_width) {
                FileOutput(f, '0');
                --fraction_width;
            }
        }
        while (-width > print_width) {
            FileOutchar(f, fill);
            width++;
        }
    }
    EXIT();
    return result != 0;
}

static HashValue
IntegerHash(Value iv)
{
    return NaturalHash(IntegerMag(iv)) ^ IntegerSign(iv);
}

static void
IntegerMark(void *object)
{
    Integer *integer = object;
    MemReference(IMag(integer));
}

ValueRep IntegerRep = {
    { IntegerMark, 0, "IntegerRep" }, /* base */
    rep_integer, /* tag */
    {
     /* binary */
        IntegerPlus, IntegerMinus,
     IntegerTimes, IntegerDivide,
     IntegerDiv, IntegerMod,
     IntegerLess, IntegerEqual,
     IntegerLand, IntegerLor,
     IntegerLxor, },
    {
     /* unary */
        IntegerNegate, IntegerFloor,
     IntegerCeil, },
    IntegerPromote,
    IntegerReduce,
    IntegerPrint,
    0,
    IntegerHash,
};

#define INTEGER_CACHE_SIZE 8191

DataCachePtr integerCache;

Value
NewInteger(Sign sign, Natural *mag)
{
    ENTER();
    unsigned c = (PtrToUInt(mag) ^ (unsigned)sign) % INTEGER_CACHE_SIZE;
    Value   *re = (Value *)DataCacheValues(integerCache) + c;
    Value    ret = *re;

    if (ret && IntegerSign(ret) == sign && NaturalEqual(mag, IntegerMag(ret))) {
        RETURN(ret);
    }
    ret = ALLOCATE(&IntegerRep.data, sizeof(Integer));
    ret->integer.magn = (Natural *)(((long)mag) | (long)sign);
    *re = ret;
    RETURN(ret);
}

Value
NewIntInteger(int i)
{
    ENTER();
    Sign          sign = Positive;
    unsigned long mag;

    if (i < 0) {
        sign = Negative;
        mag = -i;
    } else
        mag = i;
    RETURN(NewInteger(sign, NewNatural(mag)));
}

Value
NewSignedDigitInteger(signed_digit d)
{
    ENTER();
    Sign         sign = Positive;
    double_digit dd;

    if (d < 0) {
        sign = Negative;
        dd = -d;
    } else
        dd = d;
    RETURN(NewInteger(sign, NewDoubleDigitNatural(dd)));
}

/*
 * Return a count or index as an integer of any size
 */
Value
NewPtrdiffInteger(ptrdiff_t i)
{
    if (MIN_NICKLE_INT <= i && i <= MAX_NICKLE_INT)
        return NewInt((int)i);
    return NewSignedDigitInteger(i);
}

int
IntegerInit(void)
{
    ENTER();

    integerCache = NewDataCache(INTEGER_CACHE_SIZE);
    EXIT();
    return 1;
}
