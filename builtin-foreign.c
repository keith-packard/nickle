/*
 * Copyright © 2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	<errno.h>
#include	"builtin.h"

NamespacePtr	ForeignNamespace;

#if HAVE_EXTERN_SYMS

#if HAVE_DLFCN_H && HAVE_DLOPEN && HAVE_DLSYM
#define HAVE_FOREIGN_LOAD 1
#include	<dlfcn.h>

static Value
do_Foreign_load (Value av)
{
    ENTER ();
    char    *name = StrzPart (av, "invalid foreign module name");
    void    *lib;
    Value   (*init) (void);
    Value   ret = False;

    if (!name)
	RETURN (Void);
    
    lib = dlopen (name, RTLD_NOW);
    if (!lib)
    {
	char	*err = 0;
	int	e = errno;
#if HAVE_DLERROR
	err = dlerror ();
#endif
	if (!err)
	    err = "cannot open";
	RaiseStandardException (exception_open_error, 3,
				NewStrString (err),
				NewInt(e),
				av);
	RETURN (Void);
    }
    
    init = (Value (*) (void)) dlsym (lib, "nickle_init");
    if (!init)
    {
	char	*err = 0;
#if HAVE_DLERROR
	err = dlerror ();
#endif
	if (!err)
	    err = "missing nickle_init";
	RaiseStandardException (exception_open_error, 3,
				NewStrString (err), NewInt (0), av);
#if HAVE_DLCLOSE
	dlclose (lib);
#endif
	RETURN (Void);
    }
    ret = (*init) ();
    RETURN (ret);
}
#endif

#endif

void
import_Foreign_namespace()
{
    ENTER ();
    static const struct fbuiltin_1 funcs_1[] = {
#if HAVE_EXTERN_SYMS && HAVE_FOREIGN_LOAD
	{ do_Foreign_load, "load", "b", "s", "\n"
	    " bool load (string name)\n"
	    "\n"
	    " Load a foreign library into nickle\n" },
#endif
	{ 0 }
    };

    ForeignNamespace = BuiltinNamespace (/*parent*/ 0, "Foreign")->namespace.namespace;

    BuiltinFuncs1 (&ForeignNamespace, funcs_1);
    EXIT ();
}
