/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	toplevel.c
 *
 *	provide builtin functions for the Toplevel namespace
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

void
import_Toplevel_namespace()
{
    ENTER ();
    static const struct fbuiltin_0 funcs_0[] = {
        { do_getbyte, "getbyte", "i", "" },
        { do_time, "time", "i", "" },
	{ do_hash_new, "hash_new", "Hpp", "" },
        { 0 }
    };

    static const struct fbuiltin_1 funcs_1[] = {
        { do_abs, "abs", "n", "R" },
        { do_bit_width, "bit_width", "i", "i" },
        { do_ceil, "ceil", "i", "R" },
        { do_denominator, "denominator", "i", "R" },
        { do_dim, "dim", "i", "A*p" },
        { do_dims, "dims", "A*i", "Ap" },
        { do_exit, "exit", "v", "i" },
        { do_exponent, "exponent", "i", "R" },
        { do_floor, "floor", "i", "R" },
	{ do_func_args, "func_args", "i", "p" },
        { do_is_array, "is_array", "b", "p" },
        { do_is_continuation, "is_continuation", "b", "p" },
        { do_is_file, "is_file", "b", "p" },
        { do_is_func, "is_func", "b", "p" },
        { do_is_int, "is_int", "b", "p" },
        { do_is_number, "is_number", "b", "p" },
        { do_is_rational, "is_rational", "b", "p" },
        { do_is_ref, "is_ref", "b", "p" },
        { do_is_semaphore, "is_semaphore", "b", "p" },
        { do_is_string, "is_string", "b", "p" },
        { do_is_struct, "is_struct", "b", "p" },
        { do_is_thread, "is_thread", "b", "p" },
        { do_is_bool, "is_bool", "b", "p" },
        { do_is_void, "is_void", "b", "p" },
        { do_mantissa, "mantissa", "r", "R" },
        { do_numerator, "numerator", "i", "R" },
        { do_precision, "precision", "i", "R" },
        { do_profile, "profile", "b", "b" },
        { do_putbyte, "putbyte", "i", "i" },
        { do_reference, "reference", "*p", "p" },
        { do_sign, "sign", "i", "R" },
        { do_sleep, "sleep", "v", "i" },
        { do_string_to_real, "string_to_real", "R", "s" },
	{ do_hash, "hash", "i", "p" },
	{ do_hash_keys, "hash_keys", "Ap", "Hpp" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_gcd, "gcd", "i", "ii" },
        { do_setjmp, "setjmp", "p", "*cp" },
        { do_xor, "xor", "i", "ii" },
	{ do_setdims, "setdims", "v", "ApAi" },
	{ do_setdim, "setdim", "v", "Api" },
	{ do_hash_get, "hash_get", "p", "Hppp" },
	{ do_hash_del, "hash_del", "v", "Hppp" },
	{ do_hash_test, "hash_test", "b", "Hppp" },
        { 0 }
    };

    static const struct fbuiltin_2j funcs_2j[] = {
        { do_longjmp, "longjmp", "p", "cp" },
        { 0 }
    };

    static const struct fbuiltin_3 funcs_3[] = {
	{ do_hash_set, "hash_set", "v", "Hpppp" },
	{ 0 }
    };

    static const struct fbuiltin_v funcs_v[] = {
        { do_imprecise, "imprecise", "R", "R.i" },
        { do_string_to_integer, "string_to_integer", "i", "s.i" },
        { 0 }
    };

    BuiltinFuncs0 (/*parent*/ 0, funcs_0);
    BuiltinFuncs1 (/*parent*/ 0, funcs_1);
    BuiltinFuncs2 (/*parent*/ 0, funcs_2);
    BuiltinFuncs3 (/*parent*/ 0, funcs_3);
    BuiltinFuncs2J (/*parent*/ 0, funcs_2j);
    BuiltinFuncsV (/*parent*/ 0, funcs_v);
    EXIT ();
}

Value 
do_getbyte ()
{
    ENTER ();
    RETURN (do_File_getb (FileStdin));
}

Value 
do_putbyte (Value v)
{
    ENTER ();
    RETURN (do_File_putb (v, FileStdout));
}

Value 
do_gcd (Value a, Value b)
{
    ENTER ();
    RETURN (Gcd (a, b));
}

Value
do_xor (Value a, Value b)
{
    ENTER ();
    RETURN (Lxor (a, b));
}

Value
do_time (void)
{
    ENTER ();
    RETURN (NewInt (time (0)));
}

Value
do_string_to_integer (int n, Value *p)
{
    ENTER ();
    char    *s;
    int	    ibase;
    int	    negative = 0;
    Value   ret = Zero;
    Value   str = p[0];
    Value   base = Zero;
    
    switch(n) {
    case 1:
	break;
    case 2:
	base = p[1];
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"string_to_integer: wrong number of arguments",
				2,
				NewInt (2),
				NewInt (n));
	RETURN(Void);
    }
    
    s = StringChars (&str->string);
    while (isspace ((int)(*s))) s++;
    switch (*s) {
    case '-':
	negative = 1;
	s++;
	break;
    case '+':
	s++;
	break;
    }
    ibase = IntPart (base, "string_to_integer: invalid base");
    if (!aborting)
    {
	if (ibase == 0)
	{
	    if (!strncmp (s, "0x", 2) ||
		!strncmp (s, "0X", 2)) ibase = 16;
	    else if (!strncmp (s, "0t", 2) ||
		     !strncmp (s, "0T", 2)) ibase = 10;
	    else if (!strncmp (s, "0b", 2) ||
		     !strncmp (s, "0B", 2)) ibase = 2;
	    else if (!strncmp (s, "0o", 2) ||
		     !strncmp (s, "0O", 2) ||
		     *s == '0') ibase = 8;
	    else ibase = 10;
	}
	switch (ibase) {
	case 2:
	    if (!strncmp (s, "0b", 2) ||
		!strncmp (s, "0B", 2)) s += 2;
	    break;
	case 8:
	    if (!strncmp (s, "0o", 2) ||
		!strncmp (s, "0O", 2)) s += 2;
	case 10:
	    if (!strncmp (s, "0t", 2) ||
		!strncmp (s, "0T", 2)) s += 2;
	    break;
	case 16:
	    if (!strncmp (s, "0x", 2) ||
		!strncmp (s, "0X", 2)) s += 2;
	    break;
	}
	ret = atov (s, ibase);
	if (!aborting)
	{
	    if (negative)
		ret = Negate (ret);
	}
    }
    RETURN (ret);
}

Value
do_string_to_real (Value str)
{
    ENTER ();
    RETURN (aetov (StringChars (&str->string), 10));
}


Value
do_imprecise (int n, Value *p)
{
    ENTER();
    Value   v;
    int	    prec;

    v = p[0];
    if (n > 1)
    {
	prec = IntPart (p[1], "imprecise: invalid precision");
	if (prec <= 0)
	{
	    RaiseStandardException (exception_invalid_argument,
				    "imprecise: precision must be positive",
				    2, NewInt(0), p[1]);
	    RETURN(v);
	}
    }
    else
    {
	if (ValueIsFloat(v))
	    RETURN(v);
	prec = DEFAULT_FLOAT_PREC;
    }

    RETURN (NewValueFloat (v, prec));
}

Value 
do_abs (Value a)
{
    ENTER ();
    if (Negativep (a))
	a = Negate (a);
    RETURN (a);
}

Value 
do_floor (Value a)
{
    return Floor (a);
}

Value
do_func_args (Value a)
{
    ENTER ();
    if (!ValueIsFunc (a))
    {
	RaiseStandardException (exception_invalid_argument,
				"func_args: argument must be function",
				2, NewInt (0), a);
	RETURN (Void);
    }
    RETURN (NewInt (a->func.code->base.argc));
}

Value 
do_ceil (Value a)
{
    return Ceil (a);
}

Value
do_exit (Value av)
{
    ENTER ();
    int	    code;

    code = IntPart (av, "Illegal exit code");
    if (aborting)
	RETURN (Void);
    IoFini ();
    exit (code);
    RETURN (Void);
}

Value
do_dim(Value av) 
{
    ENTER();
    Value ret;
    if (av->array.ndim != 1)
    {
	RaiseStandardException (exception_invalid_argument,
				"dim: argument must be one-dimensional array",
				2, NewInt (0), av);
	RETURN (Void);
    }
    ret = NewInt(ArrayLimits(&av->array)[0]);
    RETURN (ret);
}

Value
do_dims(Value av) 
{
    ENTER();
    Value ret;
    int i;
    int ndim = av->array.ndim;

    ret = NewArray(True, False, typePrim[rep_int], 1, &ndim);
    for (i = 0; i < av->array.ndim; i++) {
      Value d = NewInt(ArrayLimits(&av->array)[i]);
      BoxValueSet(ret->array.values, i, d);
    }
    RETURN (ret);
}

Value
do_setdims (Value av, Value dv)
{
    ENTER ();
    Array   *a = &av->array;
    Array   *d = &dv->array;
    BoxPtr  db = d->values;
#define DIM_LOCAL   32
    int dimLocal[DIM_LOCAL];
    int	*dims = a->ndim < DIM_LOCAL ? dimLocal : AllocateTemp (a->ndim * sizeof (int));
    int	i;

    if (a->ndim != db->nvalues)
    {
	RaiseStandardException (exception_invalid_argument,
				"setdims: size of dimensions must match dimensionality of array",
				2, NewInt (a->ndim), dv);
	RETURN (Void);
    }
    for (i = 0; i < a->ndim; i++)
    {
	dims[i] = IntPart (BoxValueGet (db,i), "setdims: invalid dimension");
	if (aborting)
	    RETURN (Void);
	if (dims[i] < 0)
	{
	    RaiseStandardException (exception_invalid_argument,
				    "setdims: dimensions must be non-negative",
				    2, NewInt (i), NewInt (dims[i]));
	    RETURN (Void);
	}
    }
    ArraySetDimensions (av, dims);
    RETURN (Void);
}

Value
do_setdim (Value av, Value dv)
{
    ENTER ();
    int	    d = IntPart (dv, "setdim: invalid dimension");
    if (aborting)
	RETURN (Void);
    if (d < 0)
    {
	RaiseStandardException (exception_invalid_argument,
				"setdim: dimension must be non-negative",
				2, NewInt (d), Void);
	RETURN (Void);
    }
    ArrayResize (av, 0, d);
    RETURN (Void);
}
    
Value
do_reference (Value av)
{
    ENTER ();
    Value   ret;

    ret = NewRef (NewBox (False, False, 1, typePoly), 0);
    RefValueSet (ret, Copy (av));
    RETURN (ret);
}

Value
do_precision (Value av)
{
    ENTER ();
    unsigned	prec;

    if (ValueIsFloat(av))
	prec = av->floats.prec;
    else
	prec = 0;
    RETURN (NewInt (prec));
}

Value
do_sign (Value av)
{
    ENTER ();

    if (Zerop (av))
	av = Zero;
    else if (Negativep (av))
	av = NewInt(-1);
    else
	av = One;
    RETURN (av);
}

Value
do_exponent (Value av)
{
    ENTER ();
    Value   ret;

    if (!ValueIsFloat(av))
    {
	RaiseStandardException (exception_invalid_argument,
				"exponent: argument must be imprecise",
				2, NewInt (0), av);
	RETURN (Void);
    }
    ret = NewInteger (av->floats.exp->sign, av->floats.exp->mag);
    ret = Plus (ret, NewInt (FpartLength (av->floats.mant)));
    RETURN (ret);
}

Value
do_mantissa (Value av)
{
    ENTER ();
    Value   ret;

    if (!ValueIsFloat(av))
    {
	RaiseStandardException (exception_invalid_argument,
				"mantissa: argument must be imprecise",
				2, NewInt (0), av);
	RETURN (Void);
    }
    ret = NewInteger (av->floats.mant->sign, av->floats.mant->mag);
    ret = Divide (ret, Pow (NewInt (2), 
			    NewInt (FpartLength (av->floats.mant))));
    RETURN (ret);
}

Value
do_numerator (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_int:
    case rep_integer:
	break;
    case rep_rational:
	av = NewInteger (av->rational.sign, av->rational.num);
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"numerator: argument must be precise",
				2, NewInt (0), av);
	av = Void;
	break;
    }
    RETURN (av);
}

Value
do_denominator (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_int:
    case rep_integer:
	av = One;
	break;
    case rep_rational:
	av = NewInteger (Positive, av->rational.den);
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"denominator: argument must be precise",
				2, NewInt (0), av);
	av = Void;
	break;
    }
    RETURN (av);
}

Value
do_bit_width (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_int:
	av = NewInt (IntWidth (ValueInt(av)));
	break;
    case rep_integer:
	av = NewInt (NaturalWidth (IntegerMag(av)));
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"bit_width: argument must be integer",
				2, NewInt (0), av);
	av = Void;
	break;
    }
    RETURN (av);
}

Value
do_is_int (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_int:
    case rep_integer:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_rational (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_int:
    case rep_integer:
    case rep_rational:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_number (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_int:
    case rep_integer:
    case rep_rational:
    case rep_float:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_string (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_string:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_file (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_file:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_thread (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_thread:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_semaphore (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_semaphore:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_continuation (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_continuation:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_bool (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_bool:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_void (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_void:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_array (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_array:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_ref (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_ref:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_struct (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_struct:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_is_func (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case rep_func:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN (av);
}

Value
do_hash (Value a)
{
    return ValueHash (a);
}

/* hash builtins (for testing) */
Value	do_hash_new (void)
{
    return NewHash (False, typePoly, typePoly);
}

Value	do_hash_get (Value hv, Value key)
{
    return HashGet (hv, key);
}

Value	do_hash_del (Value hv, Value key)
{
    HashDelete (hv, key);
    return Void;
}

Value	do_hash_test (Value hv, Value key)
{
    return HashTest (hv, key);
}
   
Value	do_hash_keys (Value hv)
{
    return HashKeys (hv);
}

Value	do_hash_set (Value hv, Value key, Value value)
{
    HashSet (hv, key, value);
    return Void;
}
