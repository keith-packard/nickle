/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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

#ifdef MEM_TRACE
Value
do_Debug_dump_active (void);
#endif
    
void
import_Debug_namespace()
{
    ENTER ();
    static const struct fbuiltin_0 funcs_0[] = {
        { do_Debug_collect, "collect", "v", "", "\n"
	    " void collect ()\n"
	    "\n"
	    " Invokes the garbage collector\n" },
        { do_Debug_done, "done", "v", "", "\n"
	    " void done ()\n"
	    "\n"
	    " Terminates the debugger, returning control\n"
	    " to the enclosing non-debugging read/eval/print loop.\n" },
        { do_Debug_down, "down", "b", "", "\n"
	    " bool down ()\n"
	    "\n"
	    " Moves the current debug context one frame down the call chain.\n"
	    " Returns whether this was possible or not.\n" },
        { do_Debug_up, "up", "b", "", "\n"
	    " bool up ()\n"
	    "\n"
	    " Moves the current debug context one frame up the call chain.\n"
	    " Returns whether this was possible or not.\n" },
	{ do_Debug_help, "help", "v", "", "\n"
	    " void help ()\n"
	    "\n"
	    " Displays a bit of help for using the debugger.\n"	},
#ifdef MEM_TRACE
	{ do_Debug_dump_active, "dump_active", "v", "", "\n"
	    " void dump_active ()\n"
	    "\n"
	    " Dump out active memory usage.\n" },
#endif
        { 0 }
    };

    static const struct fbuiltin_1 funcs_1[] = {
        { do_Debug_dump, "dump", "v", "p", "\n"
	    " void dump (poly f)\n"
	    "\n"
	    " Dump out bytecodes for function 'f'.\n" },
        { 0 }
    };

    static const struct fbuiltin_v funcs_v[] = {
        { do_Thread_trace, "trace", "v", ".p", "\n"
	    " void trace ()\n"
	    " void trace (continuation c)\n"
	    " void trace (thread t)\n"
	    " void trace (continuation c, int depth)\n"
	    " void trace (thread t, int depth)\n"
	    "\n"
	    " Display a stack trace for a thread or continuation.\n"
	    " While debugging, there is a default continuation of the\n"
	    " thread being debugged.\n"
	    " If 'depth' is not provided, it will show 20 frames.\n" },
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
