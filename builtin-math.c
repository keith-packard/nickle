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

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr MathNamespace;

void
import_Math_namespace()
{
    ENTER ();
    static const struct fbuiltin_1 funcs_1[] = {
        { do_Math_popcount, "popcount", "i", "i", "\n"
	    " int popcount (int i)\n"
	    "\n"
	    " Return the number of '1' bits in 'i'.\n" },
	{ do_Math_factorial, "factorial", "i", "i", "\n"
	    " int factorial (int i)\n"
	    "\n"
	    " Return the factorial of 'i'.\n" },
        { 0 }
    };
    static const struct fbuiltin_2 funcs_2[] = {
        { do_Math_assignpow, "assign_pow", "n", "*Ri", "\n"
	    " real assign_pow (*real a, int b)\n"
	    "\n"
	    " Return *a ** b, also storing that result back in *a.\n" },
        { do_Math_pow, "pow", "n", "Ri", "\n"
	    " real pow (real a, int b)\n"
	    "\n"
	    " Return a ** b.\n" },
        { 0 }
    };

    MathNamespace = BuiltinNamespace (/*parent*/ 0, "Math")->namespace.namespace;

    BuiltinFuncs1 (&MathNamespace, funcs_1);
    BuiltinFuncs2 (&MathNamespace, funcs_2);
    EXIT ();
}

Value
do_Math_pow (Value a, Value b)
{
    ENTER ();
    RETURN (Pow (a, b));
}

Value
do_Math_assignpow (Value a, Value b)
{
    ENTER ();
    Value   ret;
    
    ret = Pow (RefValueGet(a), b);
    RefValueSet (a, ret);
    RETURN (ret);
}

static unsigned count_bits (unsigned n) {
    unsigned c3 = 0x0f0f0f0f;
    unsigned c2 = c3 ^ (c3 << 2);  /* c2 == 0x33333333 */
    unsigned c1 = c2 ^ (c2 << 1);  /* c1 == 0x55555555 */
    unsigned left = (n >> 1) & c1;
    n = left + (n & c1);
    left = (n >> 2) & c2;
    n = left + (n & c2);
    left = (n >> 4) & c3;
    n = left + (n & c3);
    n += (n >> 8);
    n += (n >> 16);
    return (n & 0xff);
}

Value
Popcount (Value av)
{
    ENTER ();
    Natural	*i;
    Value	ret;
    digit	*d;
    int		n;
    unsigned	part;

    if (!Integralp (ValueTag(av)))
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("Math::popcount: not an integer"),
				av, Void);
	RETURN (Void);
    }
    if (Negativep (av))
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("Math::popcount: negative argument"),
				av, Void);
	RETURN (Void);
    }
    switch (ValueTag(av)) {
    case rep_int:
	ret = NewInt (count_bits (ValueInt(av)));
	break;
    case rep_integer:
	i = IntegerMag (av);
	n = i->length;
	d = NaturalDigits(i);
	part = 0;
	ret = Zero;
	while (n--)
	{
	    if (part >= MAX_NICKLE_INT - 32)
	    {
		ret = Plus (ret, NewInt (part));
		part = 0;
		if (aborting)
		    break;
	    }
	    part += count_bits (*d++);
	}
	if (ret == Zero)
	    ret = NewInt (part);
	else
	    ret = Plus (ret, NewInt (part));
	break;
    default:
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("Math::popcount: not an integer"),
				av, Void);
	RETURN (Void);
    }
    RETURN (ret);
}

Value
do_Math_popcount(Value v) {
    ENTER ();
    RETURN (Popcount (v));
}

Value
do_Math_factorial(Value v) {
    ENTER ();
    RETURN (Factorial (v));
}
