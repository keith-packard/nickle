/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
        { do_time, "time", "i", "", "\n"
	    " int time ()\n"
	    "\n"
	    " Return seconds since Jan 1, 1970 00:00 GMT\n" },
	{ do_millis, "millis", "i", "", "\n"
	  " int millis ()\n"
	  "\n"
	  " Return time in milliseconds\n" },
        { 0 }
    };

    static const struct fbuiltin_1 funcs_1[] = {
        { do_abs, "abs", "R", "R", "\n"
	    " real abs (real r)\n"
	    "\n"
	    " Return absolute value of 'r'.\n" },
        { do_bit_width, "bit_width", "i", "i", "\n"
	    " int bit_width (int i)\n"
	    "\n"
	    " Return width of binary representation of abs('i').\n" },
        { do_ceil, "ceil", "i", "R", "\n"
	    " int ceil (real r)\n"
	    "\n"
	    " Return the nearest integer no smaller than 'r'.\n" },
        { do_denominator, "denominator", "i", "r",
	    " int denominator (rational r)\n"
	    "\n"
	    " Return the denominator of 'r'\n" },
        { do_dim, "dim", "i", "A*p", "\n"
	    " int dim (poly[*] a)\n"
	    "\n"
	    " Return the dimension of 'a'.\n" },
        { do_dims, "dims", "A*i", "Ap", "\n"
	    " int[*] dims (poly[] a)\n"
	    "\n"
	    " Return an array containing the list of dimensions of 'a'.\n" },
        { do_exit, "exit", "v", "i", "\n"
	    " void exit (int i)\n"
	    "\n"
	    " Exit from the nickle environment with code 'i'.\n" },
        { do_exponent, "exponent", "i", "R", "\n"
	    " int exponent (real r)\n"
	    "\n"
	    " Return the exponent of the imprecise value 'r'.\n" },
        { do_floor, "floor", "i", "R", "\n"
	    " int floor (real r)\n"
	    "\n"
	    " Return the nearest integer no larger than 'r'.\n" },
	{ do_func_args, "func_args", "i", "p", "\n"
	    " int func_args (poly f)\n"
	    "\n"
	    " Return the number of arguments required by function 'f'.\n" },
        { do_is_array, "is_array", "b", "p", "\n"
	    " bool is_array (poly v)\n"
	    "\n"
	    " Return whether 'v' is an array value.\n" },
        { do_is_continuation, "is_continuation", "b", "p" , "\n"
	    " bool is_continuation (poly v)\n"
	    "\n"
	    " Return whether 'v' is an continuation value.\n" },
        { do_is_file, "is_file", "b", "p" , "\n"
	    " bool is_file (poly v)\n"
	    "\n"
	    " Return whether 'v' is an file value.\n" },
        { do_is_func, "is_func", "b", "p" , "\n"
	    " bool is_func (poly v)\n"
	    "\n"
	    " Return whether 'v' is an func value.\n" },
        { do_is_hash, "is_hash", "b", "p", "\n"
	    " bool is_hash (poly v)\n"
	    "\n"
	    " Return whether 'v' is an hash value.\n" },
        { do_is_int, "is_int", "b", "p" , "\n"
	    " bool is_int (poly v)\n"
	    "\n"
	    " Return whether 'v' is an int value.\n" },
        { do_is_number, "is_number", "b", "p" , "\n"
	    " bool is_number (poly v)\n"
	    "\n"
	    " Return whether 'v' is an numeric value.\n" },
        { do_is_rational, "is_rational", "b", "p" , "\n"
	    " bool is_rational (poly v)\n"
	    "\n"
	    " Return whether 'v' is an rational value.\n" },
        { do_is_ref, "is_ref", "b", "p" , "\n"
	    " bool is_ref (poly v)\n"
	    "\n"
	    " Return whether 'v' is an ref value.\n" },
        { do_is_semaphore, "is_semaphore", "b", "p" , "\n"
	    " bool is_semaphore (poly v)\n"
	    "\n"
	    " Return whether 'v' is an semaphore value.\n" },
        { do_is_string, "is_string", "b", "p" , "\n"
	    " bool is_string (poly v)\n"
	    "\n"
	    " Return whether 'v' is an string value.\n" },
        { do_is_struct, "is_struct", "b", "p" , "\n"
	    " bool is_struct (poly v)\n"
	    "\n"
	    " Return whether 'v' is an struct value.\n" },
        { do_is_thread, "is_thread", "b", "p" , "\n"
	    " bool is_thread (poly v)\n"
	    "\n"
	    " Return whether 'v' is an thread value.\n" },
        { do_is_bool, "is_bool", "b", "p" , "\n"
	    " bool is_bool (poly v)\n"
	    "\n"
	    " Return whether 'v' is an bool value.\n" },
        { do_is_void, "is_void", "b", "p" , "\n"
	    " bool is_void (poly v)\n"
	    "\n"
	    " Return whether 'v' is an void value.\n" },
        { do_is_uninit, "is_uninit", "b", "*p", "\n"
	    " bool is_uninit (*poly r)\n"
	    "\n"
	    " Return whether 'r' references uninitialized storage.\n" },
        { do_make_uninit, "make_uninit", "v", "*p", "\n"
	    " void make_uninit (*poly r)\n"
	    "\n"
	    " Makes 'r' reference uninitialized storage.\n" },
        { do_mantissa, "mantissa", "r", "R", "\n"
	    " rational mantissa (real r)\n"
	    "\n"
	    " Return the mantissa of 'r' as a rational between 0 and 1.\n" },
        { do_numerator, "numerator", "i", "r", "\n"
	    " int numerator (rational r)\n"
	    "\n"
	    " Return the numerator of 'r'.\n" },
        { do_precision, "precision", "i", "R", "\n"
	    " int precision (real r)\n"
	    "\n"
	    " Return the number of bits in the\n"
	    " representation of the mantissa of 'r'.\n"	},
        { do_profile, "profile", "b", "b", "\n"
	    " bool profile (bool enable)\n"
	    "\n"
	    " Set profiling on/off.\n"
	    " Returns previous profiling state.\n" },
        { do_reference, "reference", "*p", "p", "\n"
	    " *poly reference (poly value)\n"
	    "\n"
	    " Returns &value.\n" },
        { do_sign, "sign", "i", "R", "\n"
	    " int sign (real r)\n"
	    " Return -1, 0, 1 for negative, zero or positive 'r'.\n" },
        { do_sleep, "sleep", "v", "i", "\n"
	    " void sleep (int milliseconds)\n"
	    "\n"
	    " Pause thread execution for 'milliseconds'.\n" },
        { do_string_to_real, "string_to_real", "R", "s", "\n"
	    " real string_to_real (string s)\n"
	    "\n"
	    " Parse a string representation of a numeric value.\n" },
	{ do_hash, "hash", "i", "p", "\n"
	    " int hash (poly p)\n"
	    "\n"
	    " Return an integer based on 'p' such that any value equal\n"
	    " to 'p' will return the same integer.\n" },
	{ do_hash_keys, "hash_keys", "Ap", "Hpp", "\n"
	    " poly[] hash_keys (poly[poly] h)\n"
	    "\n"
	    " Return an array containing all of the key in 'h'.\n"
	    " The order within the array is undefined.\n" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_gcd, "gcd", "i", "ii", "\n"
	    " int gcd (int a, int b)\n"
	    "\n"
	    " Return the greatest common divisor of 'a' and 'b'.\n" },
        { do_setjmp, "setjmp", "p", "*cp", "\n"
	    " poly setjmp (*continuation c, poly v)\n"
	    "\n"
	    " On direct invocation, create a continuation and store\n"
	    " that in *'c'.  Then return 'v'.\n"
	    " Subsequent invocations of longjmp with *'c' will appear\n"
	    " to return from 'setjmp' with the value passed to longjmp.\n" },
	{ do_setdims, "setdims", "v", "ApA*i", "\n"
	    " void setdims (poly[] a, int[*] dimensions)\n"
	    "\n"
	    " Set the dimensions of resizable array 'a' to 'dimensions'.\n"
	    " dim(dimensionss) must be the same as dim(dims(a)).\n" },
	{ do_setdim, "setdim", "v", "A.pi", "\n"
	    " void setdim (poly[...] a, int dimension)\n"
	    "\n"
	    " Set the dimension of 'a' to 'dimension'.\n" },
	{ do_hash_del, "hash_del", "v", "Hppp", "\n"
	    " void hash_del (poly[poly] h, poly key)\n"
	    "\n"
	    " Delete any hash value in 'h' associated with 'key'.\n" },
	{ do_hash_test, "hash_test", "b", "Hppp", "\n"
	    " bool hash_test (poly[poly] h, poly key)\n"
	    "\n"
	    " Return whether 'h' contains 'key'.\n" },
        { 0 }
    };

    static const struct fbuiltin_2j funcs_2j[] = {
        { do_longjmp, "longjmp", "v", "cp", "\n"
	    " void longjmp (continuation c, poly v)\n"
	    "\n"
	    " Relocate execution to that saved in 'c', making the\n"
	    " setjmp call there appear to return 'v'.\n" },
        { 0 }
    };

    static const struct fbuiltin_v funcs_v[] = {
        { do_imprecise, "imprecise", "R", "R.i", "\n"
	    " real imprecise (real r)\n"
	    " real imprecise (real r, int precision)\n"
	    "\n"
	    " Return an imprecise number.\n"
	    " The precision will be 'precision' if supplied, else 256.\n" },
        { do_string_to_integer, "string_to_integer", "i", "s.i", "\n"
	    " int string_to_integer (string s)\n"
	    " int string_to_integer (string s, int base)\n"
	    "\n"
	    " Parse 's' as an integer.\n"
	    " Use 'base' if supplied, else autodetect.\n" },
        { 0 }
    };

    BuiltinFuncs0 (/*parent*/ 0, funcs_0);
    BuiltinFuncs1 (/*parent*/ 0, funcs_1);
    BuiltinFuncs2 (/*parent*/ 0, funcs_2);
    BuiltinFuncs2J (/*parent*/ 0, funcs_2j);
    BuiltinFuncsV (/*parent*/ 0, funcs_v);
    EXIT ();
}

Value 
do_gcd (Value a, Value b)
{
    ENTER ();
    RETURN (Gcd (a, b));
}

Value
do_time (void)
{
    ENTER ();
    RETURN (Reduce (NewInteger (Positive, 
				NewDoubleDigitNatural ((double_digit) time(0)))));
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("string_to_integer: wrong number of arguments"),
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
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("imprecise: precision must be positive"),
				    NewInt(0), p[1]);
	    RETURN(v);
	}
    }
    else
    {
	Value float_prec;
	if (ValueIsFloat(v))
	    RETURN(v);
	prec = DEFAULT_FLOAT_PREC;
	float_prec = lookupVar(0, "float_precision");
	if (float_prec)
	{
		int default_prec = ValueInt(float_prec);
		if (default_prec > 1)
		    prec = default_prec;
	}
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("func_args: argument must be function"),
				NewInt (0), a);
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
    FileFini ();
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("dim: argument must be one-dimensional array"),
				NewInt (0), av);
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
    for (i = 0; i < ndim; i++) {
	Value d = NewInt(ArrayLimits(&av->array)[i]);
	ArrayValueSet(&ret->array, ndim - i - 1, d);
    }
    RETURN (ret);
}

Value
do_setdims (Value av, Value dv)
{
    ENTER ();
    Array   *a = &av->array;
    Array   *d = &dv->array;
#define DIM_LOCAL   32
    int dimLocal[DIM_LOCAL];
    int	*dims = a->ndim < DIM_LOCAL ? dimLocal : AllocateTemp (a->ndim * sizeof (int));
    int	i;

    if (a->ndim != ArrayNvalues(d))
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("setdims: size of dimensions must match dimensionality of array"),
				NewInt (a->ndim), dv);
	RETURN (Void);
    }
    if (!av->array.resizable)
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("setdims: array must be resizable"),
				av, Void);
	RETURN (Void);
    }
    for (i = 0; i < a->ndim; i++)
    {
	int j = a->ndim - 1 - i;
	dims[j] = IntPart (ArrayValue (d,i), "setdims: invalid dimension");
	if (aborting)
	    RETURN (Void);
	if (dims[j] < 0)
	{
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("setdims: dimensions must be non-negative"),
				    NewInt (i), NewInt (dims[j]));
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("setdim: dimension must be non-negative"),
				dv, Void);
	RETURN (Void);
    }
    if (!av->array.resizable)
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("setdim: array must be resizable"),
				av, Void);
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("exponent: argument must be imprecise"),
				NewInt (0), av);
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("mantissa: argument must be imprecise"),
				NewInt (0), av);
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("numerator: argument must be precise"),
				NewInt (0), av);
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("denominator: argument must be precise"),
				NewInt (0), av);
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("bit_width: argument must be integer"),
				NewInt (0), av);
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
do_is_uninit (Value av)
{
    ENTER ();
    if (!av) {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("do_is_uninit: invalid reference"),
				NewInt (0), av);
	av = Void;
    } else if (RefValueGet(av)) {
	av = FalseVal;
    } else {
	av = TrueVal;
    }
    RETURN (av);
}

Value
do_make_uninit (Value av)
{
    ENTER ();
    if (!av) {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("do_make_uninit: invalid reference"),
				NewInt (0), av);
    } else {
	RefValueSet(av, 0);
    }
    RETURN (Void);
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
do_is_hash(Value av)
{
    ENTER();
    switch(ValueTag(av)) {
    case rep_hash:
	av = TrueVal;
	break;
    default:
	av = FalseVal;
	break;
    }
    RETURN(av);
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

