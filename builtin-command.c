/* $Header$ */

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
    static const struct fbuiltin_1 funcs_1[] = {
        { do_Command_delete, "delete", "b", "s" },
        { do_Command_edit, "edit", "v", "A*s" },
	{ do_Command_display, "display", "v", "p" },
	{ do_Command_valid_name, "valid_name", "b", "A*s" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_Command_new, "new", "v", "sp" },
        { do_Command_new_names, "new_names", "v", "sp" },
        { 0 }
    };

    static const struct fbuiltin_4 funcs_4[] = {
	{ do_Command_lex_input, "lex_input", "b", "fsbb" },
        { 0 }
    };

    static const struct fbuiltin_v funcs_v[] = {
        { do_Command_undefine, "undefine", "v", ".A*s" },
        { do_Command_pretty_print, "pretty_print", "v", "f.A*s" },
        { 0 }
    };

    CommandNamespace = BuiltinNamespace (/*parent*/ 0, "Command")->namespace.namespace;

    BuiltinFuncs1 (&CommandNamespace, funcs_1);
    BuiltinFuncs2 (&CommandNamespace, funcs_2);
    BuiltinFuncs4 (&CommandNamespace, funcs_4);
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
	if (NamespaceLocate (names, &namespace, &symbol, &publish, True))
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
	if (NamespaceLocate (args[i], &namespace, &symbol, &publish, True))
	    NamespaceRemoveName (namespace, symbol->symbol.name);
    RETURN (Void);
}

Value
do_Command_valid_name (Value names)
{
    ENTER ();
    NamespacePtr    namespace;
    SymbolPtr	    symbol;
    Publish	    publish;
    Value	    ret;
    
    if (NamespaceLocate (names, &namespace, &symbol, &publish, False))
	ret = TrueVal;
    else
	ret = FalseVal;
    RETURN (ret);
}

Value
do_Command_edit (Value names)
{
    ENTER();
    NamespacePtr    namespace;
    SymbolPtr	    symbol;
    Publish	    publish;

    if (NamespaceLocate (names, &namespace, &symbol, &publish, True))
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

Value
do_Command_lex_input (Value file, Value name, Value after, Value interactive)
{
    ENTER ();
    NewLexInput (file, AtomId (StringChars (&name->string)), 
		 after == TrueVal, interactive == TrueVal);
    RETURN (TrueVal);
}
