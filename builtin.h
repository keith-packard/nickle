/* $Header$ */

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	builtin.h
 *
 *	header shared across builtin implementation source files
 */

#include	"nickle.h"

SymbolPtr
BuiltinAddName (NamespacePtr	*namespacep,
		SymbolPtr	symbol);

SymbolPtr
BuiltinSymbol (NamespacePtr *namespacep,
	       char	    *string,
	       Type	    *type);

SymbolPtr
BuiltinNamespace (NamespacePtr  *namespacep,
		  char		*string);

SymbolPtr
BuiltinException (NamespacePtr  *namespacep,
		  char		*string,
		  Type		*type,
		  char		*doc);

struct ebuiltin {
    char		*name;
    StandardException	exception;
    char		*args;
    char		*doc;
};

void
BuiltinAddException (NamespacePtr	*namespacep, 
		     StandardException	exception,
		     char		*name,
		     char		*format,
		     char		*doc);

void
BuiltinAddFunction (NamespacePtr *namespacep, char *name, char *ret_format,
		    char *format, BuiltinFunc f, Bool jumping, char *doc);

#define BuiltinFuncStructDef(stype, functype) \
    struct stype { \
        functype func; \
        char *name; \
        char *ret; \
        char *args; \
	char *doc; \
    }

#define BuiltinFuncsGeneric(ns, funcs, stype, bitem, jump) do { \
    BuiltinFunc f; const struct stype *fi; \
    for (fi = (funcs); fi->name; fi++) { \
	f.bitem = fi->func; \
	BuiltinAddFunction ((ns), fi->name, fi->ret, fi->args, f, jump, fi->doc); \
    } } while (0)

typedef Value (*fbuiltin_v_func) (int, Value *);
BuiltinFuncStructDef(fbuiltin_v, fbuiltin_v_func);
#define BuiltinFuncsV(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_v, builtinN, False)

typedef Value (*fbuiltin_0_func) (void);
BuiltinFuncStructDef(fbuiltin_0, fbuiltin_0_func);
#define BuiltinFuncs0(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_0, builtin0, False)

typedef Value (*fbuiltin_1_func) (Value);
BuiltinFuncStructDef(fbuiltin_1, fbuiltin_1_func);
#define BuiltinFuncs1(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_1, builtin1, False)

typedef Value (*fbuiltin_2_func) (Value, Value);
BuiltinFuncStructDef(fbuiltin_2, fbuiltin_2_func);
#define BuiltinFuncs2(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_2, builtin2, False)

typedef Value (*fbuiltin_3_func) (Value, Value, Value);
BuiltinFuncStructDef(fbuiltin_3, fbuiltin_3_func);
#define BuiltinFuncs3(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_3, builtin3, False)

typedef Value (*fbuiltin_4_func) (Value, Value, Value, Value);
BuiltinFuncStructDef(fbuiltin_4, fbuiltin_4_func);
#define BuiltinFuncs4(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_4, builtin4, False)

typedef Value (*fbuiltin_5_func) (Value, Value, Value, Value, Value);
BuiltinFuncStructDef(fbuiltin_5, fbuiltin_5_func);
#define BuiltinFuncs5(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_5, builtin5, False)

typedef Value (*fbuiltin_6_func) (Value, Value, Value, Value, Value, Value);
BuiltinFuncStructDef(fbuiltin_6, fbuiltin_6_func);
#define BuiltinFuncs6(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_6, builtin6, False)

typedef Value (*fbuiltin_7_func) (Value, Value, Value, Value, Value, Value, Value);
BuiltinFuncStructDef(fbuiltin_7, fbuiltin_7_func);
#define BuiltinFuncs7(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_7, builtin7, False)

typedef Value (*fbuiltin_8_func) (Value, Value, Value, Value, Value, Value, Value, Value);
BuiltinFuncStructDef(fbuiltin_8, fbuiltin_8_func);
#define BuiltinFuncs8(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_8, builtin8, False)

typedef Value (*fbuiltin_vj_func) (InstPtr *, int, Value *);
BuiltinFuncStructDef(fbuiltin_vj, fbuiltin_vj_func);
#define BuiltinFuncsVJ(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_vj, builtinNJ, True)

typedef Value (*fbuiltin_0j_func) (InstPtr *);
BuiltinFuncStructDef(fbuiltin_0j, fbuiltin_0j_func);
#define BuiltinFuncs0J(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_0j, builtin0J, True)

typedef Value (*fbuiltin_1j_func) (InstPtr *, Value);
BuiltinFuncStructDef(fbuiltin_1j, fbuiltin_1j_func);
#define BuiltinFuncs1J(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_1j, builtin1J, True)

typedef Value (*fbuiltin_2j_func) (InstPtr *, Value, Value);
BuiltinFuncStructDef(fbuiltin_2j, fbuiltin_2j_func);
#define BuiltinFuncs2J(n, f) \
	BuiltinFuncsGeneric(n, f, fbuiltin_2j, builtin2J, True)

#include "builtin-namespaces.h"
