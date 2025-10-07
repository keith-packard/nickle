/*
 * Copyright © 2025 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"builtin.h"

NamespacePtr	ComplexNamespace;

Value do_Complex_creal (Value z);
Value do_Complex_cimag (Value z);

void
import_Complex_namespace()
{
    ENTER();
    static const struct fbuiltin_1 funcs_1[] = {
	{ do_Complex_creal, "creal", "N", "C", "\n"
	  " real creal(complex c)\n"
	  "\n"
	  " Returns the real component of a complex value\n" },
	{ do_Complex_cimag, "cimag", "N", "C", "\n"
	  " real cimag(complex c)\n"
	  "\n"
	  " Returns the imaginary component of a complex value\n" },
	{ 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
	{ NewValueComplex, "cmplx", "C", "NN", "\n"
	  " complex cmplx(real r, real i)\n"
	  "\n"
	  " Returns a new complex value given real and imaginary components\n" },
	{ 0 },
    };

    ComplexNamespace = BuiltinNamespace(/*parent*/ 0, "Complex")->namespace.namespace;

    BuiltinFuncs1 (&ComplexNamespace, funcs_1);
    BuiltinFuncs2 (&ComplexNamespace, funcs_2);
    EXIT();
}

Value do_Complex_creal (Value z)
{
    if (ValueIsComplex(z))
	return z->complex.r;
    return z;
}

Value do_Complex_cimag (Value z)
{
    if (ValueIsComplex(z))
	return z->complex.i;
    return Zero;
}
