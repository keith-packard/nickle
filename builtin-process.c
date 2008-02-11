/*
 * Copyright © 1988-2008 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * builtin-process.c
 *
 * provide builtin functions for the Process namespace
 */

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <grp.h>

#include "builtin.h"

NamespacePtr ProcessNamespace;

static Value
do_Process_getuid (void)
{
    ENTER ();
    RETURN (NewInt (getuid()));
}

static Value
do_Process_geteuid (void)
{
    ENTER ();
    RETURN (NewInt (geteuid()));
}

static Value
do_Process_getgid (void)
{
    ENTER ();
    RETURN (NewInt (getgid()));
}

static Value
do_Process_getegid (void)
{
    ENTER ();
    RETURN (NewInt (getegid()));
}

static Value
do_Process_getgroups (void)
{
    ENTER ();
    int	    n;
    gid_t   *list;
    Value   ret;
    int	    i;

    n = getgroups (0, NULL);
    list = AllocateTemp (n * sizeof (gid_t));
    getgroups (n, list);
    ret = NewArray (False, False, typePrim[rep_integer], 1, &n);
    for (i = 0; i < n; i++)
	ArrayValueSet(&ret->array, i, NewInt (list[i]));
    RETURN (ret);
}

static Value
do_Process_getpid (void)
{
    ENTER ();
    RETURN (NewInt (getpid()));
}

static Value
error (Value value)
{
    int	err = errno;

    RaiseStandardException (exception_system_error, 3,
			    FileGetErrorMessage (err),
			    NewInt (err), value);
    return Void;
}

static Value
do_Process_setuid (Value uid)
{
    ENTER ();
    int u = IntPart (uid, "Invalid uid");
    if (aborting)
	RETURN(Void);
    
    if (setuid (u) < 0)
	RETURN (error (uid));
	
    RETURN (Void);
}

static Value
do_Process_seteuid (Value euid)
{
    ENTER ();
    int u = IntPart (euid, "Invalid euid");
    if (aborting)
	RETURN(Void);
    
    if (seteuid (u) < 0)
	RETURN (error (euid));
	
    RETURN (Void);
}

static Value
do_Process_setgid (Value gid)
{
    ENTER ();
    int u = IntPart (gid, "Invalid gid");
    if (aborting)
	RETURN(Void);
    
    if (setgid (u) < 0)
	RETURN (error (gid));
	
    RETURN (Void);
}

static Value
do_Process_setegid (Value egid)
{
    ENTER ();
    int u = IntPart (egid, "Invalid egid");
    if (aborting)
	RETURN(Void);
    
    if (setegid (u) < 0)
	RETURN (error (egid));
	
    RETURN (Void);
}

static Value
do_Process_setgroups (Value groups)
{
    ENTER ();
    int	    n;
    int	    i;
    gid_t   *g;

    n = ArrayLimits (&groups->array)[0];
    g = AllocateTemp (n * sizeof (gid_t));
    for (i = 0; i < n; i++) {
	g[i] = IntPart (ArrayValueGet (&groups->array, i), "Invalid gid");
	if (aborting)
	    RETURN(Void);
    }
    
    if (setgroups (n, g) < 0)
	RETURN (error (groups));
	
    RETURN (Void);
}

void
import_Process_namespace (void)
{
    ENTER ();

    static const struct fbuiltin_0 funcs_0[] = {
	{ do_Process_getuid, "getuid", "i", "", "\n"
	    " int getuid ()\n"
	    "\n"
	    " Return the current uid\n" },
	{ do_Process_geteuid, "geteuid", "i", "", "\n"
	    " int geteuid ()\n"
	    "\n"
	    " Return the current effective uid\n" },
	{ do_Process_getgid, "getgid", "i", "", "\n"
	    " int getgid ()\n"
	    "\n"
	    " Return the current gid\n" },
	{ do_Process_getegid, "getegid", "i", "", "\n"
	    " int getegid ()\n"
	    "\n"
	    " Return the current effective gid\n" },
	{ do_Process_getgroups, "getgroups", "Ai", "", "\n"
	    " int[*] getgroups ()\n"
	    "\n"
	    " Return the list of additional groups\n" },
	{ do_Process_getpid, "getpid", "i", "", "\n"
	    " int getpid ()\n"
	    "\n"
	    " Return the current process id." },
	{ 0 }
    };
    static const struct fbuiltin_1 funcs_1[] = {
	{ do_Process_setuid, "setuid", "v", "i", "\n"
	    " void setuid (int uid)\n"
	    "\n"
	    " Set the current uid." },
	{ do_Process_seteuid, "seteuid", "v", "i", "\n"
	    " void seteuid (int euid)\n"
	    "\n"
	    " Set the current euid." },
	{ do_Process_setgid, "setgid", "v", "i", "\n"
	    " void setgid (int gid)\n"
	    "\n"
	    " Set the current gid." },
	{ do_Process_setegid, "setegid", "v", "i", "\n"
	    " void setegid (int egid)\n"
	    "\n"
	    " Set the current egid." },
	{ do_Process_setgroups, "setgroups", "v", "Ai", "\n"
	    " void setgroups (int[*] groups)\n"
	    "\n"
	    " Set the list of additional groups." },
	{ 0 }
    };

    static const struct ebuiltin excepts[] = {
	{"system_error",    	exception_system_error,		"sEp", "\n"
	    " system_error (string message, error_type error, poly value)\n"
	    "\n"
	    " Raised when a system function fails.\n"
	    " 'message' is a printable error string.\n"
	    " 'error' is a symbolic error code.\n"
	    " 'value' is the value which failed.\n" },
	{ 0, 0 },
    };
    const struct ebuiltin   *e;
	
    ProcessNamespace = BuiltinNamespace (/*parent*/ 0, "Process")->namespace.namespace;
    
    for (e = excepts; e->name; e++)
	BuiltinAddException (&ProcessNamespace, e->exception, e->name, e->args, e->doc);

    BuiltinFuncs0 (&ProcessNamespace, funcs_0);
    BuiltinFuncs1 (&ProcessNamespace, funcs_1);
    EXIT ();
}

    
