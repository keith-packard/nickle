/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	command.c
 *
 *	provide builtin functions for the Command namespace
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr CommandNamespace;

void
import_Command_namespace()
{
    ENTER ();
    static struct fbuiltin_1 funcs_1[] = {
        { do_Command_delete, "delete", "b", "s" },
        { do_Command_edit, "edit", "v", "A*s" },
        { do_Command_lex_file, "lex_file", "b", "s" },
        { do_Command_lex_library, "lex_library", "b", "s" },
        { do_Command_lex_string, "lex_string", "v", "s" },
	{ do_Command_display, "display", "v", "p" },
        { 0 }
    };

    static struct fbuiltin_2 funcs_2[] = {
        { do_Command_new, "new", "v", "sp" },
        { do_Command_new_names, "new_names", "v", "sp" },
        { 0 }
    };

    static struct fbuiltin_v funcs_v[] = {
        { do_Command_undefine, "undefine", "v", ".A*s" },
        { do_Command_pretty_print, "pretty_print", "v", "f.A*s" },
        { 0 }
    };

    CommandNamespace = BuiltinNamespace (/*parent*/ 0, "Command")->namespace.namespace;

    BuiltinFuncs1 (&CommandNamespace, funcs_1);
    BuiltinFuncs2 (&CommandNamespace, funcs_2);
    BuiltinFuncsV (&CommandNamespace, funcs_v);
    EXIT ();
}

static Value
do_Command_new_common (Value name, Value func, Bool names)
{
    ENTER();
    char	*cmd;
    int		c;
    
    if (!ValueIsFunc(func))
    {
	RaiseStandardException (exception_invalid_argument,
				"argument must be func",
				2, NewInt (1), func);
	RETURN (Void);
    }
    cmd = StringChars (&name->string);
    while ((c = *cmd++))
    {
	if (isupper (c) || islower (c))
	    continue;
	if (cmd != StringChars (&name->string) + 1)
	{
	    if (isdigit ((int)c) || c == '_')
	     continue;
	}
	RaiseStandardException (exception_invalid_argument,
				"argument must be valid name",
				2, NewInt (0), name);
	RETURN (Void);
    }
    CurrentCommands = NewCommand (CurrentCommands, 
				  AtomId (StringChars (&name->string)),
				  func, names);
    RETURN (Void);
}

Value
do_Command_new (Value name, Value func)
{
    return do_Command_new_common (name, func, False);
}

Value
do_Command_new_names (Value name, Value func)
{
    return do_Command_new_common (name, func, True);
}

Value
do_Command_delete (Value name)
{
    ENTER();
    Atom    id;

    id = AtomId (StringChars (&name->string));
    if (!CommandFind (CurrentCommands, id))
	RETURN (FalseVal);
    CurrentCommands = CommandRemove (CurrentCommands, id);
    RETURN (TrueVal);
}

Value
do_Command_lex_file (Value name)
{
    ENTER ();
    Value   r;

    if (LexFile (StringChars (&name->string), False, False))
	r = TrueVal;
    else
	r = FalseVal;
    RETURN (r);
}

Value
do_Command_lex_library (Value name)
{
    ENTER ();
    Value   r;

    if (LexLibrary (StringChars (&name->string), False, False))
	r = TrueVal;
    else
	r = FalseVal;
    RETURN (r);
}

Value
do_Command_lex_string (Value name)
{
    ENTER();

    LexString (StringChars (&name->string), False);
    RETURN (Void);
}

Value
do_Command_pretty_print (int argc, Value *args)
{
    ENTER();
    Value	    f;
    Value	    names;
    NamespacePtr    namespace;
    SymbolPtr	    symbol;
    Publish	    publish;
    int		    i;

    f = args[0];
    for (i = 1; i < argc; i++)
    {
	names = args[i];
	if (NamespaceLocate (names, &namespace, &symbol, &publish) && symbol)
	    PrettyPrint (f, publish, symbol);
    }
    RETURN (Void);
}

Value
do_Command_undefine (int argc, Value *args)
{
    ENTER ();
    NamespacePtr    namespace;
    SymbolPtr	    symbol;
    Publish	    publish;
    int		    i;
    
    for (i = 0; i < argc; i++)
	if (NamespaceLocate (args[i], &namespace, &symbol, &publish) && symbol)
	    NamespaceRemoveName (namespace, symbol->symbol.name);
    RETURN (Void);
}

Value
do_Command_edit (Value names)
{
    ENTER();
    NamespacePtr    namespace;
    SymbolPtr	    symbol;
    Publish	    publish;

    if (NamespaceLocate (names, &namespace, &symbol, &publish) && symbol)
	EditFunction (symbol, publish); 
    RETURN (Void);
}

Value
do_Command_display (Value v)
{
    ENTER ();
    FilePrintf (FileStdout, "%v\n", v);
    RETURN (Void);
}
