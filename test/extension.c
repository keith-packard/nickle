/*
 * Copyright © 2026 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/* A C extension for extensiontest.5c, counting how often it initializes */

#include "builtin.h"

Value      nickle_init(void);

static int extensionInits;

static Value
do_ExtensionTest_inits(void)
{
    return NewInt(extensionInits);
}

Value
nickle_init(void)
{
    ENTER();
    static const struct fbuiltin_0 funcs_0[] = {
        { do_ExtensionTest_inits, "inits", "i", "",
         "\n"
          " int inits ()\n"
          "\n"
          " How many times this extension has initialized\n" },
        { 0 }
    };
    NamespacePtr namespace = BuiltinNamespace(/*parent*/ 0, "ExtensionTest")->namespace.namespace;

    extensionInits++;
    BuiltinFuncs0(&namespace, funcs_0);
    RETURN(TrueVal);
}
