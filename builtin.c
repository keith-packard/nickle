/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	builtin.c
 *
 *	initialize builtin functions
 */

#include	"builtin.h"

struct sbuiltin {
    char	    *value;
    char	    *name;
    NamespacePtr    *namespace;
};

struct envbuiltin {
    char	    *var;
    char	    *def;
    char	    *name;
    NamespacePtr    *namespace;
};
    
struct filebuiltin {
    char	    *name;
    Value   	    *value;
    NamespacePtr    *namespace;
};

struct ebuiltin {
    char		*name;
    StandardException	exception;
    char		*args;
    NamespacePtr	*namespace;
};

static struct sbuiltin svars[] = {
    { "> ",	    "prompt" },
    { "+ ",	    "prompt2" },
    { "- ",	    "prompt3" },
    { "%g",	    "format" },
    { VERSION,	    "version" },
#ifdef BUILD
    { BUILD,	    "build" },
#else
    { "?",	    "build" },
#endif
    { 0,    0 },
};

extern NamespacePtr CommandNamespace;

static struct envbuiltin envvars[] = {
    { "NICKLELIB",  NICKLELIB,	"library_path",	&CommandNamespace },
    { 0,    0 },
};

static struct filebuiltin fvars[] = {
    { "stdin",	&FileStdin },
    { "stdout",	&FileStdout },
    { "stderr",	&FileStderr },
    { 0,	0 },
};

static struct ebuiltin excepts[] = {
    {"uninitialized_value",	exception_uninitialized_value,	"s" },
    {"invalid_argument",	exception_invalid_argument,	"sip" },
    {"readonly_box",		exception_readonly_box,		"sp" },
    {"invalid_array_bounds",	exception_invalid_array_bounds,	"spp" },
    {"divide_by_zero",		exception_divide_by_zero,	"snn" },
    {"invalid_struct_member",	exception_invalid_struct_member,"sps" },
    {"invalid_binop_values",	exception_invalid_binop_values,	"spp" },
    {"invalid_unop_value",	exception_invalid_unop_value,	"sp" },
    {0,				0 },
};

SymbolPtr
BuiltinAddName (NamespacePtr	*namespacep,
		SymbolPtr	symbol)
{
    ENTER ();
    NamespacePtr    namespace;

    if (namespacep)
	namespace = *namespacep;
    else
	namespace = GlobalNamespace;
    RETURN(NamespaceAddName (namespace, symbol, publish_public));
}

SymbolPtr
BuiltinSymbol (NamespacePtr *namespacep,
	       char	    *string,
	       Types	    *type)
{
    ENTER ();
    RETURN (BuiltinAddName (namespacep, 
			    NewSymbolGlobal (AtomId (string),
					     type)));
}

static char *
BuiltinType (char *format, Types **type)
{
    Types   *t;
    Bool    ref = False;
    Bool    array = False;
    Expr    *dims = 0;
    
    ref = False;
    if (*format == '*')
    {
	ref = True;
	format++;
    }
    if (*format == 'A')
    {
	array = True;
	format++;
	while (*format == '*')
	{
	    dims = NewExprTree (COMMA, 0, dims);
	    format++;
	}
    }
    switch (*format++) {
    case 'p': t = typesPoly; break;
    case 'n': t = typesGroup; break;
    case 'N': t = typesField; break;
    case 'E': t = typesFileError; break;
    case 'R': t = typesPrim[type_float]; break;
    case 'r': t = typesPrim[type_rational]; break;
    case 'i': t = typesPrim[type_integer]; break;
    case 's': t = typesPrim[type_string]; break;
    case 'f': t = typesPrim[type_file]; break;
    case 't': t = typesPrim[type_thread]; break;
    case 'S': t = typesPrim[type_semaphore]; break;
    case 'c': t = typesPrim[type_continuation]; break;
    case 'v': t = typesPrim[type_void]; break;
    default: 
	t = 0;
	write (2, "Invalid builtin argument type\n", 30);
	break;
    }
    if (ref)
	t = NewTypesRef (t);
    if (array)
	t = NewTypesArray (t, dims);
    *type = t;
    return format;
}
	     
static ArgType *
BuiltinArgTypes (char *format, int *argcp)
{
    ENTER ();
    ArgType	*args, *a, **last;
    int		argc;
    Types	*t;
    Bool	varargs;
    
    args = 0;
    last = &args;
    argc = 0;
    while (*format)
    {
	varargs = False;
	if (*format == '.')
	{
	    varargs = True;
	    format++;
	}
	format = BuiltinType (format, &t);
	if (!varargs)
	    argc++;
        a = NewArgType (t, varargs, 0, 0, 0);
	*last = a;
	last = &a->next;
    }
    *argcp = argc;
    RETURN(args);
}

SymbolPtr
BuiltinNamespace (NamespacePtr  *namespacep,
		  char		*string)
{
    ENTER ();
    RETURN (BuiltinAddName (namespacep, 
			    NewSymbolNamespace (AtomId (string),
						NewNamespace (GlobalNamespace))));
}

SymbolPtr
BuiltinException (NamespacePtr  *namespacep,
		  char		*string,
		  Types		*type)
{
    ENTER ();
    RETURN (BuiltinAddName (namespacep, 
			    NewSymbolException (AtomId (string), type)));
}

void
BuiltinAddException (NamespacePtr	*namespacep, 
		     StandardException	exception,
		     char		*name,
		     char		*format)
{
    ENTER ();
    SymbolPtr	sym;
    ArgType	*args;
    Types	*type;
    int		argc;

    args = BuiltinArgTypes (format, &argc);
    type = NewTypesFunc (typesPoly, args);
    sym = BuiltinException (namespacep, name, NewTypesFunc (typesPoly, args));
    RegisterStandardException (exception, sym);
    EXIT ();
}

void
BuiltinAddFunction (NamespacePtr *namespacep, char *name, char *ret_format,
		    char *format, BuiltinFunc f, Bool jumping)
{
    ENTER ();
    Value	func;
    SymbolPtr	sym;
    int		argc;
    ArgType	*args;
    Types	*ret;

    args = BuiltinArgTypes (format, &argc);
    BuiltinType (ret_format, &ret);
    sym = BuiltinSymbol (namespacep, name, NewTypesFunc (ret, args));
    func =  NewFunc (NewBuiltinCode (ret, args, argc, f, jumping), 0);
    BoxValueSet (sym->global.value, 0, func);
    EXIT ();
}

void
BuiltinInit (void)
{
    ENTER ();
    struct sbuiltin	*s;
    struct envbuiltin	*env;
    struct filebuiltin	*f;
    struct ebuiltin	*e;
    SymbolPtr		sym;
    char		*home;
    Value		home_val;

    /* Import standard namespaces (and their contents :) */
    import_Toplevel_namespace();
    import_Debug_namespace();
    import_File_namespace();
    import_Math_namespace();
#ifdef BSD_RANDOM
    import_BSDRandom_namespace();
#endif
    import_Semaphore_namespace();
    import_String_namespace();
    import_Thread_namespace();
    import_Command_namespace();
#ifdef GCD_DEBUG
    import_Gcd_namespace();
#endif
    import_Environ_namespace();
		import_Sockets_namespace();

    /* Import builtin strings with predefined values */
    for (s = svars; s->name; s++) {
	sym = BuiltinSymbol (s->namespace, s->name, typesPrim[type_string]);
	BoxValueSet (sym->global.value, 0, NewStrString (s->value));
    }

    /* Get the user's home directory in case it's referenced in the
     * environment */
    home = getenv ("HOME");
    if (!home)
	home = "/tmp";
    if (home[0] == '/' && home[1] == '\0')
	home = "";
    home_val = NewStrString (home);

    /* Import builtin strings from the environment */
    for (env = envvars; env->name; env++) {
	char	*v;
	Value	val;
	sym = BuiltinSymbol (env->namespace, env->name, typesPrim[type_string]);
	v = getenv (env->var);
	if (!v)
	    v = env->def;
	if (*v == '~')
	    val = Plus (home_val, NewStrString (v + 1));
	else
	    val = NewStrString (v);
	BoxValueSet (sym->global.value, 0, val);
    }

    /* Import File objects with predefined values */
    for (f = fvars; f->name; f++) {
	sym = BuiltinSymbol (f->namespace, f->name, typesPrim[type_file]);
	BoxValueSet (sym->global.value, 0, *f->value);
    }

    /* Import standard exceptions */
    for (e = excepts; e->name; e++)
	BuiltinAddException (e->namespace, e->exception, e->name, e->args);

    EXIT ();
}
