/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
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
    static struct fbuiltin_1 funcs_1[] = {
        { do_Environ_check, "check", "b", "s" },
        { do_Environ_get, "get", "s", "s" },
        { do_Environ_unset, "unset", "b", "s" },
        { 0 }
    };

    static struct fbuiltin_2 funcs_2[] = {
        { do_Environ_set, "set", "b", "ss" },
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
    char    *c;

    c = getenv (StringChars (&av->string));
    if (!c) {
	RaiseStandardException (exception_invalid_argument,
				"name not available",
				2, NewInt(0), av);
	RETURN (Void);
    }
    RETURN (NewStrString (c));
}

Value
do_Environ_check (Value av)
{
    ENTER ();
    char    *c;

    c = getenv (StringChars (&av->string));
    if (c)
	RETURN (TrueVal);
    RETURN (FalseVal);
}

extern char **environ;

Value
do_Environ_unset (Value av)
{
    ENTER ();
    char *name = StringChars(&av->string);
    int n = strlen(name);
    while(*environ) {
	if (!strncmp(*environ, name, n) && (*environ)[n] == '=') {
	    char **tail = environ;
	    while(*(tail+1))
		tail++;
	    *environ = *tail;
	    *tail = 0;
	    RETURN(TrueVal);
	}
	environ++;
    }
    RETURN (FalseVal);
}

Value
do_Environ_set (Value name, Value value)
{
    ENTER ();
    Value binding = NewString(strlen (StringChars(&name->string)) + 1 +
			      strlen (StringChars(&value->string)));
    (void) strcpy (StringChars(&binding->string),
		   StringChars(&name->string));
    (void) strcat (StringChars(&binding->string), "=");
    (void) strcat (StringChars(&binding->string),
		   StringChars(&value->string));
    if (putenv (StringChars (&binding->string)) < 0)
	RETURN (FalseVal);
    RETURN (TrueVal);
}
