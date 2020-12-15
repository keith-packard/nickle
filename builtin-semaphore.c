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
        { do_Semaphore_signal, "signal", "v", "S", "\n"
	    " void signal (semaphore s)\n"
	    "\n"
	    " Increment the count in 's' by one.\n"
	    " If the count is <= 0, wakeup one thread waiting on 's'.\n" },
        { do_Semaphore_count, "count", "i", "S", "\n"
	    " int count (semaphore s)\n"
	    "\n"
	    " Return current semaphore count\n" },
        { do_Semaphore_test, "test", "b", "S", "\n"
	    " bool test (semaphore s)\n"
	    "\n"
	    " Return false if a 'wait' call would block, else\n"
	    " 'wait' on 's' and return true.\n" },
        { do_Semaphore_wait, "wait", "v", "S", "\n"
	    " void wait (semaphore s)\n"
	    "\n"
	    " Decrement the count in 's' by one.\n"
	    " If the count is < 0, wait until awoken when 's' is signalled.\n" },
        { 0 }
    };

    static const struct fbuiltin_v funcs_v[] = {
        { do_Semaphore_new, "new", "S", ".i", "\n"
	    " semaphore new ()\n"
	    " semaphore new (int init)\n"
	    "\n"
	    " Create a new semaphore.\n"
	    " Set the initial count to 'init' if provided, else 0.\n" },
        { 0 }
    };

    SemaphoreNamespace = BuiltinNamespace (/*parent*/ 0, "Semaphore")->namespace.namespace;

    BuiltinFuncs1 (&SemaphoreNamespace, funcs_1);
    BuiltinFuncsV (&SemaphoreNamespace, funcs_v);
    EXIT ();
}
