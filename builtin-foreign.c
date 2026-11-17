/*
 * Copyright © 2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include <ctype.h>
#include <strings.h>
#include <time.h>
#include <errno.h>
#include "builtin.h"

static NamespacePtr ForeignNamespace;

#ifdef HAVE_EXTERN_SYMS

#if defined(HAVE_DLFCN_H) && defined(HAVE_DLOPEN) && defined(HAVE_DLSYM)
#define HAVE_FOREIGN_LOAD 1
#include <dlfcn.h>

/*
 * The libraries Foreign::load has initialized, by their dlopen handles,
 * which are the same however a library is named, so each initializes
 * only once
 */
static void    **foreignLoaded;
static ptrdiff_t foreignLoadedCount;

static Value
do_Foreign_load(Value av)
{
    ENTER();
    char *name = StrzPart(av, "invalid foreign module name");
    void *lib;
    Value (*module_init)(void);
    Value ret;

    if (!name)
        RETURN(Void);

    /* dlopen needn't set errno; dlerror's message has the details */
    errno = 0;
    lib = dlopen(name, RTLD_NOW);
    if (!lib) {
        char *err = 0;
        int   e = errno ? errno : EINVAL;
#ifdef HAVE_DLERROR
        err = dlerror();
#endif
        if (!err)
            err = "cannot open";
        RaiseStandardException(exception_open_error, 3, NewStrString(err), FileGetError(e), av);
        RETURN(Void);
    }

    /* A library loaded before keeps what its first load set up */
    for (ptrdiff_t i = 0; i < foreignLoadedCount; i++)
        if (foreignLoaded[i] == lib) {
#ifdef HAVE_DLCLOSE
            dlclose(lib);
#endif
            RETURN(TrueVal);
        }

    module_init = (Value (*)(void))dlsym(lib, "nickle_init");
    if (!module_init) {
        char *err = 0;
#ifdef HAVE_DLERROR
        err = dlerror();
#endif
        if (!err)
            err = "missing nickle_init";
        RaiseStandardException(exception_open_error, 3, NewStrString(err), FileGetError(EINVAL),
                               av);
#ifdef HAVE_DLCLOSE
        dlclose(lib);
#endif
        RETURN(Void);
    }
    void **loaded = realloc(foreignLoaded, (size_t)(foreignLoadedCount + 1) * sizeof(void *));
    if (!loaded)
        MemExhausted();
    foreignLoaded = loaded;
    foreignLoaded[foreignLoadedCount++] = lib;
    ret = (*module_init)();
    RETURN(ret);
}
#endif

#endif

#ifndef HAVE_FOREIGN_LOAD
/* Without dlopen, no library loads, so the library command reports that */
static Value
do_Foreign_load(Value av)
{
    ENTER();
    RaiseStandardException(exception_open_error, 3,
                           NewStrString("loading C extensions isn't supported here"),
                           FileGetError(ENOSYS), av);
    RETURN(Void);
}
#endif

void
import_Foreign_namespace(void)
{
    ENTER();
    static const struct fbuiltin_1 funcs_1[] = {
        { do_Foreign_load, "load", "b", "s",
         "\n"
          " bool load (string name)\n"
          "\n"
          " Load a C extension, a shared library, into nickle,\n"
          " initializing it only the first time\n" },
        { 0 }
    };

    ForeignNamespace = BuiltinNamespace(/*parent*/ 0, "Foreign")->namespace.namespace;

    BuiltinFuncs1(&ForeignNamespace, funcs_1);
    EXIT();
}
