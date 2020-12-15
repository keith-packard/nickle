/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	gcd.c
 *
 *	provide builtin functions for the Gcd namespace
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr GcdNamespace;

void
import_Gcd_namespace()
{
    ENTER ();
    static struct fbuiltin_2 funcs_2[] = {
        { do_Gcd_bdivmod, "bdivmod", "i", "ii" },
        { do_Gcd_kary_reduction, "kary_reduction", "i", "ii" },
        { 0 }
    };

    GcdNamespace = BuiltinNamespace (/*parent*/ 0, "Gcd")->namespace.namespace;

    BuiltinFuncs2 (&GcdNamespace, funcs_2);
    EXIT ();
}

#ifdef GCD_DEBUG
Value 
do_Gcd_bdivmod (Value a, Value b)
{
    ENTER ();
    RETURN (Bdivmod (a, b));
}

Value 
do_Gcd_kary_reduction (Value a, Value b)
{
    ENTER ();
    RETURN (KaryReduction (a, b));
}
#endif
