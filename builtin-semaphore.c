/* $Header$ */

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	semaphore.c
 *
 *	provide builtin functions for the Semaphore namespace
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr SemaphoreNamespace;

void
import_Semaphore_namespace()
{
    ENTER ();
    static const struct fbuiltin_1 funcs_1[] = {
        { do_Semaphore_signal, "signal", "v", "S" },
        { do_Semaphore_test, "test", "b", "S" },
        { do_Semaphore_wait, "wait", "v", "S" },
        { 0 }
    };

    static const struct fbuiltin_v funcs_v[] = {
        { do_Semaphore_new, "new", "S", ".i" },
        { 0 }
    };

    SemaphoreNamespace = BuiltinNamespace (/*parent*/ 0, "Semaphore")->namespace.namespace;

    BuiltinFuncs1 (&SemaphoreNamespace, funcs_1);
    BuiltinFuncsV (&SemaphoreNamespace, funcs_v);
    EXIT ();
}
