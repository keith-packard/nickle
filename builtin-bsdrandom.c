/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	bsdrandom.c
 *
 *	provide builtin functions for the BSDRandom namespace
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr BSDRandomNamespace;

void
import_BSDRandom_namespace()
{
    ENTER ();
    static struct fbuiltin_1 funcs_1[] = {
        { do_BSD_random, "random", "i", "i" },
        { do_BSD_srandom, "srandom", "i", "i" },
        { 0 }
    };

    BSDRandomNamespace = BuiltinNamespace (/*parent*/ 0, "BSDRandom")->namespace.namespace;

    BuiltinFuncs1 (&BSDRandomNamespace, funcs_1);
    EXIT ();
}

#ifdef BSD_RANDOM

Value
do_BSD_random (Value bits)
{
    ENTER();
    int n = IntPart (bits, "random: modulus non-integer");
    Value ret = Zero;

    if (n > 31)
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("random: modulus exceeds 2^31"),
				NewInt (0), bits);
    else if (n <= 0)
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("random: bad modulus"),
				NewInt (0), bits);
    else
	ret = NewInt (random () & ((1 << n) - 1));
    RETURN (ret);
}

Value
do_BSD_srandom (Value seed)
{
    ENTER();
    int n = IntPart (seed, "srandom: non-integer seed");

    srandom ((unsigned int) n);
    RETURN (seed);
}
