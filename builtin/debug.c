/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	debug.c
 *
 *	provide builtin functions for the Debug namespace
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr DebugNamespace;

void
import_Debug_namespace()
{
    ENTER ();
    static struct fbuiltin_0 funcs_0[] = {
        { do_Debug_collect, "collect", "v", "" },
        { do_Debug_done, "done", "v", "" },
        { do_Debug_down, "down", "b", "" },
        { do_Debug_up, "up", "b", "" },
	{ do_Debug_help, "help", "v", ""},
        { 0 }
    };

    static struct fbuiltin_1 funcs_1[] = {
        { do_Debug_dump, "dump", "v", "p" },
        { 0 }
    };

    static struct fbuiltin_v funcs_v[] = {
        { do_Thread_trace, "trace", "v", ".p" },
        { 0 }
    };

    DebugNamespace = BuiltinNamespace (/*parent*/ 0, "Debug")->namespace.namespace;

    BuiltinFuncs0 (&DebugNamespace, funcs_0);
    BuiltinFuncs1 (&DebugNamespace, funcs_1);
    BuiltinFuncsV (&DebugNamespace, funcs_v);
    EXIT ();
}

Value
do_Debug_collect (void)
{
    ENTER ();
    MemCollect ();
    RETURN (Void);
}
