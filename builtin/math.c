/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
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
        { do_Math_popcount, "popcount", "i", "i" },
        { 0 }
    };
    static const struct fbuiltin_2 funcs_2[] = {
        { do_Math_assignpow, "assign_pow", "n", "*Ri" },
        { do_Math_pow, "pow", "n", "Ri" },
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
    
    ret = Pow (RefValue (a), b);
    RefValueSet (a, ret);
    RETURN (ret);
}

static unsigned count_bits(unsigned n) {
    int i;
    int count = 0;

    for (i = 0; i < 32; i++) {
	count += (n & 1);
	n >>= 1;
    }
    return count;
}

Value
Popcount (Value av)
{
    ENTER ();
    Natural *i, *count, *mask;

    if (!Integralp (ValueTag(av)))
    {
	RaiseError ("popcount: non-integer argument %v", av);
	RETURN (Void);
    }
    if (Negativep (av))
    {
	RaiseError ("popcount: negative argument %v", av);
	RETURN (Void);
    }
    switch (ValueTag(av)) {
    case rep_int:
	count = NewNatural (count_bits (ValueInt (av)));
	break;
    case rep_integer:
	count = zero_natural;
	i = IntegerMag (av);
	mask = NewNatural (0xffffffff);
	while (!NaturalZero (i)) {
	    int bitcount = count_bits (NaturalToInt (NaturalLand (i, mask)));
	    count = NaturalPlus (count, NewNatural (bitcount));
	    i = NaturalRsl (i, 32);
	}
	break;
    default:
	RaiseError ("popcount: uncaught non-integer argument %v", av);
	RETURN (Void);
    }
    RETURN (NewInteger (Positive, count));
}

Value
do_Math_popcount(Value v) {
    ENTER ();
    RETURN (Popcount (v));
}
