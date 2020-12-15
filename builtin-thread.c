/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
    static const struct fbuiltin_0 funcs_0[] = {
        { do_Thread_current, "current", "t", "", "\n"
	    " thread current ()\n"
	    "\n"
	    " Return the current thread\n" },
        { do_Thread_list, "list", "v", "", "\n"
	    " void list ()\n"
	    "\n"
	    " Display a list of the known threads.\n" },
        { 0 }
    };

    static const struct fbuiltin_1 funcs_1[] = {
        { do_Thread_get_priority, "get_priority", "i", "t", "\n"
	    " int get_priority (thread t)\n"
	    "\n"
	    " Return the scheduling priority for 't'.\n" },
        { do_Thread_id_to_thread, "id_to_thread", "t", "i", "\n"
	    " thread id_to_thread (int id)\n"
	    "\n"
	    " Return the thread identified by 'id'.\n" },
        { do_Thread_join, "join", "p", "t", "\n"
	    " poly join (thread t)\n"
	    "\n"
	    " Await the completion of 't' and return its return value.\n" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_Thread_set_priority, "set_priority", "i", "ti", "\n"
	    " int set_priority (thread t, int priority)\n"
	    "\n"
	    " Set 't's scheduling priority to 'priority'.\n"
	    " Return 'priority.\n" },
	{ do_Thread_signal, "send_signal", "v", "ti", "\n"
	    " void signal (thread t, int signal)\n"
	    "\n"
	    " Raise the signal exception in thread 't'\n"
	    " passing 'signal' as the argument\n" },
        { 0 }
    };

    static const struct fbuiltin_v funcs_v[] = {
        { do_Thread_kill, "kill", "i", ".t", "\n"
	    " int kill (thread t ...)\n"
	    "\n"
	    " If no threads are provided, kill a thread being debugged.\n"
	    " Otherwise, kill all of the threads provided.\n"
	    " Return the number of threads killed that weren't already\n"
	    " finished.\n" },
        { 0 }
    };

    static const struct ebuiltin excepts[] = {
	{"signal",	exception_signal,	"i", "\n"
	    " signal (int signal)\n"
	    "\n"
	    " Sent from the Thread::send_signal function.\n" },
	{ 0 },
    };

    ThreadNamespace = BuiltinNamespace (/*parent*/ 0, "Thread")->namespace.namespace;

    BuiltinFuncs0 (&ThreadNamespace, funcs_0);
    BuiltinFuncs1 (&ThreadNamespace, funcs_1);
    BuiltinFuncs2 (&ThreadNamespace, funcs_2);
    BuiltinFuncsV (&ThreadNamespace, funcs_v);
    BuiltinExceptions (&ThreadNamespace, excepts);
    EXIT ();
}
