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

static unsigned count_bits (unsigned n) {
    unsigned c3 = 0x0f0f0f0f;
    unsigned c2 = c3 ^ (c3 << 2);
    unsigned c1 = c2 ^ (c2 << 1);
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
	ret = NewInt (count_bits (ValueInt(av)));
	break;
    case rep_integer:
	i = IntegerMag (av);
	n = i->length;
	d = NaturalDigits(i);
	/*
	 * If the result will certainly fit in an int, use one of those
	 */
	if (n < MAX_NICKLE_INT >> LLBASE2)
	{
	    unsigned t = 0;
	    while (n--)
		t += count_bits(*d++);
	    ret = NewInt(t);
	}
	else
	{
	    ret = Zero;
	    while (n--)
		ret = Plus (ret, NewInt (count_bits (*d++)));
	}
	break;
    default:
	RaiseError ("popcount: uncaught non-integer argument %v", av);
	RETURN (Void);
    }
    RETURN (ret);
}

Value
do_Math_popcount(Value v) {
    ENTER ();
    RETURN (Popcount (v));
}
