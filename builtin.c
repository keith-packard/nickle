/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	builtin.c
 *
 *	initialize builtin functions
 */

#include	"builtin.h"

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
    { "stdin",	&FileStdinBox },
    { "stdout",	&FileStdoutBox },
    { "stderr",	&FileStderrBox },
    { 0,	0 },
};

static const struct ibuiltin ivars[] = {
    { DEFAULT_FLOAT_PREC, "float_precision", &GlobalNamespace },
    { 0,    0 },
};

static const struct ebuiltin excepts[] = {
    {"uninitialized_value",	exception_uninitialized_value,	"s", "\n"
	" uninitialized_value (string message)\n"
	"\n"
	" Attempting to fetch from uninitialized storage.\n"
	" 'message' indicates the error context.\n" },
    {"invalid_argument",	exception_invalid_argument,	"sip", "\n"
	" invalid_argument (string message, int id, poly value)\n"
	"\n"
	" Function argument 'id' couldn't accept 'value'.\n"
	" 'message' indicates the error context.\n" },
    {"readonly_box",		exception_readonly_box,		"sp", "\n"
	" readonly_box (string message, poly value)\n"
	"\n"
	" Attempting to store 'value' in const storage.\n"
	" 'message' indicates the error context.\n" },
    {"invalid_array_bounds",	exception_invalid_array_bounds,	"spp", "\n"
	" invalid_array_bounds (string message, poly box, poly index)\n"
	"\n"
	" Attempt to index outside of array or do pointer arithmetic\n"
	" on a pointer not referencing an array.\n"
	" 'message' indicates the error context.\n" },
    {"divide_by_zero",		exception_divide_by_zero,	"RR", "\n"
	" divide_by_zero (real num, real den)\n"
	"\n"
	" Division or modulus by zero.\n" },
    {"invalid_struct_member",	exception_invalid_struct_member,"ps", "\n"
	" invalid_struct_member (poly value, string member)\n"
	"\n"
	" 'member' is not in 'value'.\n" },
    {"invalid_binop_values",	exception_invalid_binop_values,	"spp",
	" invalid_binop_values (string message, poly left, poly right)\n"
	"\n"
	" 'left' and 'right' aren't compatible with a binary operator.\n"
	" 'message' indicates which operator is problematic.\n" },
    {"invalid_unop_value",	exception_invalid_unop_value,	"sp", "\n"
	" invalid_unop_value (string message, poly value)\n"
	"\n"
	" 'value' isn't compatible with a unary operator.\n"
	" 'message' indicates which operator is problematic.\n" },
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

static SymbolPtr
BuiltinSymbolValue(NamespacePtr *namespacep,
		   char	    *string,
		   Type	    *type,
		   BoxPtr   value)
{
    ENTER ();
    RETURN (BuiltinAddName (namespacep, 
			    NewSymbolGlobalValue (AtomId (string),
						  value)));
}

static Type *typeUserdef[100];

void
BuiltinSetUserdefType (Type *type, int n)
{
    typeUserdef[n] = type;
}

static char *
BuiltinType (char *format, Type **type, Bool arg)
{
    Type   *t;
    Bool    ref = False;
    Bool    array = False;
    Bool    hash = False;
    Bool    resizable = False;
    Expr    *dims = 0;
    Type    *k;
    char    f;
    int	    i;
    
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
	while (*format == '*' || *format == '.')
	{
	    if (*format == '.')
		resizable = True;
	    dims = NewExprComma (0, dims);
	    format++;
	}
    }
    if (*format == 'H')
    {
	hash = True;
	format = BuiltinType (format + 1, &k, True);
    }
    switch (f = *format++) {
    case 'p': t = typePoly; break;
    case 'n': t = typePrim[rep_float]; break;
    case 'N': t = typePrim[rep_float]; break;
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
    case 'F': t = typePrim[rep_foreign]; break;
    case 'a': t = typeSockaddr; break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
	i = (f - '0') * 10 + (*format++) - '0';
	t = typeUserdef[i];
	if (t)
	    break;
    default: 
	t = 0;
	i = write (2, "Invalid builtin argument type\n", 30);
	(void) i;
	break;
    }
    if (ref)
	t = NewTypeRef (t, arg ? False : True);
    if (array)
	t = NewTypeArray (t, dims, resizable);
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
	format = BuiltinType (format, &t, True);
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
		  Type		*type,
		  char		*doc)
{
    ENTER ();
    Value   doc_value = doc ? NewStrString (doc) : Void;
    
    RETURN (BuiltinAddName (namespacep, 
			    NewSymbolException (AtomId (string),
						type, doc_value)));
}

void
BuiltinAddException (NamespacePtr	*namespacep, 
		     StandardException	exception,
		     char		*name,
		     char		*format,
		     char		*doc)
{
    ENTER ();
    SymbolPtr	sym;
    ArgType	*args;
    Type	*type;
    int		argc;

    args = BuiltinArgType (format, &argc);
    type = NewTypeFunc (typePoly, args);
    sym = BuiltinException (namespacep, name,
			    type,
			    doc);
    RegisterStandardException (exception, sym);
    EXIT ();
}

void
BuiltinAddFunction (NamespacePtr *namespacep, char *name, char *ret_format,
		    char *format, BuiltinFunc f, Bool jumping, char *doc)
{
    ENTER ();
    Value	func;
    SymbolPtr	sym;
    int		argc;
    ArgType	*args;
    Type	*ret;

    args = BuiltinArgType (format, &argc);
    BuiltinType (ret_format, &ret, False);
    sym = BuiltinSymbol (namespacep, name, NewTypeFunc (ret, args));
    func =  NewFunc (NewBuiltinCode (ret, args, argc, f, jumping, doc), 0);
    BoxValueSet (sym->global.value, 0, func);
    EXIT ();
}

void
BuiltinInit (void)
{
    ENTER ();
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
    import_Foreign_namespace ();
    import_PID_namespace ();
    import_Date_namespace();

    /* Import builtin strings with predefined values */
    BuiltinStrings (svars);

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
	sym = BuiltinSymbolValue (f->namespace, f->name, typePrim[rep_file], *f->box);
    }

    /* Import int objects with predefined values */
    BuiltinIntegers (ivars);

    /* Import standard exceptions */
    for (e = excepts; e->name; e++)
	BuiltinAddException (0, e->exception, e->name, e->args, e->doc);

    EXIT ();
}
