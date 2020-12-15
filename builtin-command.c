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
        { do_Command_delete, "delete", "b", "s", "\n"
	    " bool delete(string command)\n"
	    "\n"
	    " Remove 'command' from the set of valid commands\n" },
        { do_Command_edit, "edit", "v", "A*s", "\n"
	    " void edit(string[*] function)\n"
	    "\n"
	    " Invoke $EDITOR on 'function', parse the file when done\n" },
	{ do_Command_display, "display", "v", "p", "\n"
	    " void display (poly value)\n"
	    "\n"
	    " Built-in display primitive for read/eval/print loop" },
	{ do_Command_valid_name, "valid_name", "b", "A*s", "\n"
	    " bool valid_name (string[*] name)\n"
	    "\n"
	    " Check for a symbol table entry\n" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_Command_new, "new", "v", "sp", "\n"
	    " void new (string name, poly f)\n"
	    "\n"
	    " Create a new command which calls 'f'.\n"
	    " 'f' will be invoked with an array of values\n" },
        { do_Command_new_names, "new_names", "v", "sp", "\n"
	    " void new_names (string name, poly f)\n"
	    "\n"
	    " Create a new command which calls 'f' with literal arguments.\n"
	    " 'f' will be invoked with an array of strings\n" },
        { 0 }
    };

    static const struct fbuiltin_4 funcs_4[] = {
	{ do_Command_lex_input, "lex_input", "b", "fsbb", "\n"
	    " bool lex_input (file f, string name, bool after, bool interactive)"
	    "\n"
	    " Add 'f' to the list of files to be read by the lexer.\n"
	    " 'name' will be used when reporting parse errors.\n"
	    " If 'after', the specified file will be read when all other\n"
	    " input sources are exhausted.\n"
	    " If 'interactive', the lexer will display prompts as if\n"
	    " input was coming from a terminal.\n" },
        { 0 }
    };

    static const struct fbuiltin_v funcs_v[] = {
        { do_Command_undefine, "undefine", "v", ".A*s", "\n"
	    " void undefine (string[*] ... name)\n"
	    "\n"
	    " removes 'name' from its namespace\n" },
        { do_Command_pretty_print, "pretty_print", "v", "f.A*s", "\n"
	    " void pretty_print (file f, string[*] ... name)\n"
	    "\n"
	    " Prints the variable and value in a format capable of\n"
	    " reproducing the value if fed back to the parser\n" },
	
        { 0 }
    };

    CommandNamespace = BuiltinNamespace (/*parent*/ 0, "Command")->namespace.namespace;

    BuiltinFuncs1 (&CommandNamespace, funcs_1);
    BuiltinFuncs2 (&CommandNamespace, funcs_2);
    BuiltinFuncs4 (&CommandNamespace, funcs_4);
    BuiltinFuncsV (&CommandNamespace, funcs_v);
    EXIT ();
}

static char *
command_name (Value name)
{
    char    *cmd_base = StrzPart (name, "argument must be valid name");
    char    *cmd = cmd_base;
    int	    c;
    
    if (!cmd_base)
	return 0;
    while ((c = *cmd++))
    {
	if (isupper (c) || islower (c))
	    continue;
	if (cmd != cmd_base + 1)
	{
	    if (isdigit ((int)c) || c == '_')
	     continue;
	}
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("argument must be valid name"),
				NewInt (0), name);
	return 0;
    }
    return cmd_base;
}

static Value
do_Command_new_common (Value name, Value func, Bool names)
{
    ENTER();
    char    *cmd = command_name (name);
    
    if (!cmd)
	RETURN (Void);
    if (!ValueIsFunc(func))
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("argument must be func"),
				NewInt (1), func);
	RETURN (Void);
    }
    CurrentCommands = NewCommand (CurrentCommands, AtomId (cmd),
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
    char    *cmd = command_name (name);

    if (!cmd)
	RETURN (Void);
    id = AtomId (cmd);
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
    if (argc == 1) {
	PrettyPrint (f, publish_public, 0);
	RETURN (Void);
    }
    for (i = 1; i < argc; i++)
    {
	names = args[i];
	if (NamespaceLocate (names, &namespace, &symbol, &publish, False))
	    PrettyPrint (f, publish, symbol);
	else
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("name not found"),
				    NewInt (i), names);
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
