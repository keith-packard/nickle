/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	history.c
 *
 *	provide builtin functions for the History namespace
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr HistoryNamespace;

void
import_History_namespace()
{
    ENTER ();
    static struct fbuiltin_1 funcs_1[] = {
        { do_History_insert, "insert", "p", "p" },
        { 0 }
    };

    static struct fbuiltin_v funcs_v[] = {
        { do_History_show, "show", "i", "s.i" },
        { 0 }
    };

    HistoryNamespace = BuiltinNamespace (/*parent*/ 0, "History")->namespace.namespace;

    BuiltinFuncs1 (&HistoryNamespace, funcs_1);
    BuiltinFuncsV (&HistoryNamespace, funcs_v);
    EXIT ();
}

Value
do_History_insert (Value a)
{
    ENTER ();
    HistoryInsert (a);
    complete = True;
    RETURN (a);
}

Value
do_History_show (int n, Value *p)
{
    ENTER ();

    switch (n) {
    case 1:
	HistoryDisplay (p[0], (Value *) 0, (Value *) 0);
	break;
    case 2:
	HistoryDisplay (p[0], p + 1, (Value *) 0);
	break;
    case 3:
	HistoryDisplay (p[0], p + 1, p + 2);
	break;
    }
    RETURN (Zero);
}
