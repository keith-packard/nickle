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
#include	"builtin/version.h"

struct sbuiltin {
    char	    *value;
    char	    *name;
    NamespacePtr    *namespace;
};

struct envbuiltin {
#ifdef CENVIRON
    char	    *var;
#endif
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

static const struct sbuiltin svars[] = {
    { "> ",	    "prompt" },
    { "+ ",	    "prompt2" },
    { "- ",	    "prompt3" },
    { "%g",	    "format" },
#ifdef BUILD_VERSION
    { BUILD_VERSION, "version" },
#else
    { VERSION, "version" },
#endif
#ifdef BUILD
    { BUILD,	    "build" },
#else
    { "?",	    "build" },
#endif
    { 0,    0 },
};

extern NamespacePtr CommandNamespace;
extern Type	    *typeSockaddr;

static const struct envbuiltin envvars[] = {
#ifdef CENVIRON
    { "NICKLEPATH",  NICKLEPATH,	"nickle_path",	&CommandNamespace },
#else
    { NICKLELIBDIR,	"nickle_libdir",	&CommandNamespace },
#endif
    { 0,    0 },
};

static const struct filebuiltin fvars[] = {
    { "stdin",	&FileStdin },
    { "stdout",	&FileStdout },
    { "stderr",	&FileStderr },
    { 0,	0 },
};

static const struct ebuiltin excepts[] = {
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
	       Type	    *type)
{
    ENTER ();
    RETURN (BuiltinAddName (namespacep, 
			    NewSymbolGlobal (AtomId (string),
					     type)));
}

static char *
BuiltinType (char *format, Type **type)
{
    Type   *t;
    Bool    ref = False;
    Bool    array = False;
    Bool    hash = False;
    Expr    *dims = 0;
    Type    *k;
    
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
	    dims = NewExprComma (0, dims);
	    format++;
	}
    }
    if (*format == 'H')
    {
	hash = True;
	format = BuiltinType (format + 1, &k);
    }
    switch (*format++) {
    case 'p': t = typePoly; break;
    case 'n': t = typeGroup; break;
    case 'N': t = typeField; break;
    case 'E': t = typeFileError; break;
    case 'R': t = typePrim[rep_float]; break;
    case 'r': t = typePrim[rep_rational]; break;
    case 'i': t = typePrim[rep_integer]; break;
    case 's': t = typePrim[rep_string]; break;
    case 'f': t = typePrim[rep_file]; break;
    case 't': t = typePrim[rep_thread]; break;
    case 'S': t = typePrim[rep_semaphore]; break;
    case 'c': t = typePrim[rep_continuation]; break;
    case 'b': t = typePrim[rep_bool]; break;
    case 'v': t = typePrim[rep_void]; break;
    case 'a': t = typeSockaddr; break;
    default: 
	t = 0;
	write (2, "Invalid builtin argument type\n", 30);
	break;
    }
    if (ref)
	t = NewTypeRef (t, False);
    if (array)
	t = NewTypeArray (t, dims);
    if (hash)
	t = NewTypeHash (t, k);
    *type = t;
    return format;
}
	     
static ArgType *
BuiltinArgType (char *format, int *argcp)
{
    ENTER ();
    ArgType	*args, *a, **last;
    int		argc;
    Type	*t;
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
		  Type		*type)
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
    Type	*type;
    int		argc;

    args = BuiltinArgType (format, &argc);
    type = NewTypeFunc (typePoly, args);
    sym = BuiltinException (namespacep, name, NewTypeFunc (typePoly, args));
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
    Type	*ret;

    args = BuiltinArgType (format, &argc);
    BuiltinType (ret_format, &ret);
    sym = BuiltinSymbol (namespacep, name, NewTypeFunc (ret, args));
    func =  NewFunc (NewBuiltinCode (ret, args, argc, f, jumping), 0);
    BoxValueSet (sym->global.value, 0, func);
    EXIT ();
}

void
BuiltinInit (void)
{
    ENTER ();
    const struct sbuiltin	*s;
    const struct filebuiltin	*f;
    const struct ebuiltin	*e;
    SymbolPtr			sym;
    const struct envbuiltin	*env;
#ifdef CENVIRON
    char			*home;
    Value			home_val;
#endif

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
    import_Socket_namespace();

    /* Import builtin strings with predefined values */
    for (s = svars; s->name; s++) {
	sym = BuiltinSymbol (s->namespace, s->name, typePrim[rep_string]);
	BoxValueSet (sym->global.value, 0, NewStrString (s->value));
    }

#ifdef CENVIRON
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
	sym = BuiltinSymbol (env->namespace, env->name, typePrim[rep_string]);
	v = getenv (env->var);
	if (!v)
	    v = env->def;
	if (*v == '~')
	    val = Plus (home_val, NewStrString (v + 1));
	else
	    val = NewStrString (v);
	BoxValueSet (sym->global.value, 0, val);
    }
#else
    /* export builtin strings */
    for (env = envvars; env->name; env++) {
	Value	val;
	sym = BuiltinSymbol (env->namespace, env->name, typePrim[rep_string]);
	val = NewStrString (env->def);
	BoxValueSet (sym->global.value, 0, val);
    }
#endif

    /* Import File objects with predefined values */
    for (f = fvars; f->name; f++) {
	sym = BuiltinSymbol (f->namespace, f->name, typePrim[rep_file]);
	BoxValueSet (sym->global.value, 0, *f->value);
    }

    /* Import standard exceptions */
    for (e = excepts; e->name; e++)
	BuiltinAddException (e->namespace, e->exception, e->name, e->args);

    EXIT ();
}
