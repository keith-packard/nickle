/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	environ.c
 *
 *	provide builtin functions for the Environ namespace
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr EnvironNamespace;

void
import_Environ_namespace()
{
    ENTER ();
    static const struct fbuiltin_1 funcs_1[] = {
        { do_Environ_check, "check", "b", "s", "\n"
	    " bool check (string name)\n"
	    "\n"
	    " Test whether 'name' is in the environment\n" },
        { do_Environ_get, "get", "s", "s", "\n"
	    " string get (string name)\n"
	    "\n"
	    " Return a value from the environment.\n"
	    " raise invalid_argument if 'name' isn't in the environment.\n" },
        { do_Environ_unset, "unset", "b", "s", "\n"
	    " bool unset (string name)\n"
	    "\n"
	    " Remove 'name' from the environment.\n"
	    " Returns 'true' if 'name' was in the environment.\n" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_Environ_set, "set", "b", "ss", "\n"
	    " bool set (string name, string value)\n"
	    "\n"
	    " Places 'name' with 'value' in the environment.\n"
	    " Returns false if it didn't work for some reason.\n" },
        { 0 }
    };

    EnvironNamespace = BuiltinNamespace (/*parent*/ 0, "Environ")->namespace.namespace;

    BuiltinFuncs1 (&EnvironNamespace, funcs_1);
    BuiltinFuncs2 (&EnvironNamespace, funcs_2);
    EXIT ();
}

Value
do_Environ_get (Value av)
{
    ENTER ();
    char    *name = StrzPart (av, "invalid environment variable name");
    char    *c;

    if (!name)
	RETURN (Void);
    c = getenv (name);
    if (!c) {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("name not available"),
				NewInt(0), av);
	RETURN (Void);
    }
    RETURN (NewStrString (c));
}

Value
do_Environ_check (Value av)
{
    ENTER ();
    char    *name = StrzPart (av, "invalid environment variable name");

    if (!name)
	RETURN (Void);
    if (getenv (name))
	RETURN (TrueVal);
    RETURN (FalseVal);
}

Value
do_Environ_unset (Value av)
{
    ENTER ();
    char    *name = StrzPart (av, "invalid environment variable name");
    
    if (!name)
	RETURN (Void);
    
#if HAVE_UNSETENV
    if (getenv (name))
    {
	unsetenv (name);
	RETURN (TrueVal);
    }
#endif
    RETURN (FalseVal);
}

Value
do_Environ_set (Value name, Value value)
{
    ENTER ();
    char    *n = StrzPart (name, "invalid environment variable name");
    char    *v = StrzPart (value, "invalid environment variable value");

    if (!n || !v)
	RETURN (Void);
#if HAVE_SETENV
    if (setenv (n, v, 1) >= 0)
	RETURN (TrueVal);
#else
#if HAVE_PUTENV
    {
	Value	binding = Plus (name,
				Plus (NewStrString ("="),
				      value));
	char	*b = StrzPart (binding, "invalid environment variable binding");

	if (!b)
	    RETURN (Void);
	if (putenv (b) >= 0)
	    RETURN (TrueVal);
    }
#endif
#endif
    RETURN (FalseVal);
}
