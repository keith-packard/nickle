/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	thread.c
 *
 *	provide builtin functions for the Thread namespace
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr ThreadNamespace;

void
import_Thread_namespace()
{
    ENTER ();
    static struct fbuiltin_0 funcs_0[] = {
        { do_Thread_cont, "cont", "i", "" },
        { do_Thread_current, "current", "t", "" },
        { do_Thread_list, "list", "v", "" },
        { 0 }
    };

    static struct fbuiltin_1 funcs_1[] = {
        { do_Thread_get_priority, "get_priority", "i", "t" },
        { do_Thread_id_to_thread, "id_to_thread", "t", "i" },
        { do_Thread_join, "join", "p", "t" },
        { 0 }
    };

    static struct fbuiltin_2 funcs_2[] = {
        { do_Thread_set_priority, "set_priority", "i", "ti" },
        { 0 }
    };

    static struct fbuiltin_v funcs_v[] = {
        { do_Thread_kill, "kill", "i", ".t" },
        { 0 }
    };

    ThreadNamespace = BuiltinNamespace (/*parent*/ 0, "Thread")->namespace.namespace;

    BuiltinFuncs0 (&ThreadNamespace, funcs_0);
    BuiltinFuncs1 (&ThreadNamespace, funcs_1);
    BuiltinFuncs2 (&ThreadNamespace, funcs_2);
    BuiltinFuncsV (&ThreadNamespace, funcs_v);
    EXIT ();
}
