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
    static struct fbuiltin_2 funcs_2[] = {
        { do_Math_assignpow, "assign_pow", "n", "*Ri" },
        { do_Math_pow, "pow", "n", "Ri" },
        { 0 }
    };

    MathNamespace = BuiltinNamespace (/*parent*/ 0, "Math")->namespace.namespace;

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
