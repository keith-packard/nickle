/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	math.c
 *
 *	provide builtin functions for the Math namespace
 */

#include <ctype.h>
#include <strings.h>
#include <time.h>
#include "builtin.h"

NamespacePtr MathNamespace;

void
import_Math_namespace(void)
{
    ENTER();
    static const struct fbuiltin_1 funcs_1[] = {
        { do_Math_popcount, "popcount", "i", "i",
         "\n"
          " int popcount (int i)\n"
          "\n"
          " Return the number of '1' bits in 'i'.\n" },
        { do_Math_factorial, "factorial", "i", "i",
         "\n"
          " int factorial (int i)\n"
          "\n"
          " Return the factorial of 'i'.\n" },
        { 0 }
    };
    static const struct fbuiltin_2 funcs_2[] = {
        { do_Math_assignpow, "assign_pow", "n", "*Ri",
         "\n"
          " real assign_pow (*real a, int b)\n"
          "\n"
          " Return *a ** b, also storing that result back in *a.\n" },
        { do_Math_pow, "pow", "n", "Ri",
         "\n"
          " real pow (real a, int b)\n"
          "\n"
          " Return a ** b.\n" },
        { do_Math_ldexp, "ldexp", "R", "Ri",
         "\n"
          " real ldexp (real r, int e)\n"
          "\n"
          " Return 'r' * 2 ** 'e'. For imprecise 'r', this adjusts\n"
          " the exponent without computing 2 ** 'e'.\n" },
        { 0 }
    };

    MathNamespace = BuiltinNamespace(/*parent*/ 0, "Math")->namespace.namespace;

    BuiltinFuncs1(&MathNamespace, funcs_1);
    BuiltinFuncs2(&MathNamespace, funcs_2);
    EXIT();
}

Value
do_Math_pow(Value a, Value b)
{
    ENTER();
    RETURN(Pow(a, b));
}

Value
do_Math_ldexp(Value a, Value e)
{
    ENTER();

    if (ValueIsFloat(a))
        RETURN(FloatLdexp(a, e));
    RETURN(Times(a, Pow(NewInt(2), e)));
}

Value
do_Math_assignpow(Value a, Value b)
{
    ENTER();
    Value ret;

    ret = Pow(RefValueGet(a), b);
    RefValueSet(a, ret);
    RETURN(ret);
}

/* Count the set bits of 'n', clearing the lowest one each step */
static int
count_bits(double_digit n)
{
    int c = 0;

    while (n) {
        n &= n - 1;
        c++;
    }
    return c;
}

Value
Popcount(Value av)
{
    ENTER();
    Natural  *i;
    Value     ret;
    digit    *d;
    ptrdiff_t n;
    int       part;

    if (!Integralp(ValueTag(av))) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("Math::popcount: not an integer"), av, Void);
        RETURN(Void);
    }
    if (Negativep(av)) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("Math::popcount: negative argument"), av, Void);
        RETURN(Void);
    }
    switch (ValueTag(av)) {
    case rep_int:
        ret = NewInt(count_bits((double_digit)ValueInt(av)));
        break;
    case rep_integer:
        i = IntegerMag(av);
        n = i->length;
        d = NaturalDigits(i);
        part = 0;
        ret = Zero;
        while (n--) {
            if (part >= MAX_NICKLE_INT - 32) {
                ret = Plus(ret, NewInt(part));
                part = 0;
                if (aborting)
                    break;
            }
            part += count_bits(*d++);
        }
        if (ret == Zero)
            ret = NewInt(part);
        else
            ret = Plus(ret, NewInt(part));
        break;
    default:
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("Math::popcount: not an integer"), av, Void);
        RETURN(Void);
    }
    RETURN(ret);
}

Value
do_Math_popcount(Value v)
{
    ENTER();
    RETURN(Popcount(v));
}

Value
do_Math_factorial(Value v)
{
    ENTER();
    RETURN(Factorial(v));
}
