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
    static struct fbuiltin_0 funcs_0[] = {
        { do_getchar, "getchar", "i", "" },
        { do_time, "time", "i", "" },
        { 0 }
    };

    static struct fbuiltin_1 funcs_1[] = {
        { do_abs, "abs", "n", "R" },
        { do_bit_width, "bit_width", "i", "i" },
        { do_ceil, "ceil", "i", "R" },
        { do_denominator, "denominator", "i", "R" },
        { do_dim, "dim", "i", "A*p" },
        { do_dims, "dims", "A*i", "Ap" },
        { do_exit, "exit", "v", "i" },
        { do_exponent, "exponent", "i", "R" },
        { do_floor, "floor", "i", "R" },
        { do_is_array, "is_array", "i", "p" },
        { do_is_continuation, "is_continuation", "i", "p" },
        { do_is_file, "is_file", "i", "p" },
        { do_is_func, "is_func", "i", "p" },
        { do_is_int, "is_int", "i", "p" },
        { do_is_number, "is_number", "i", "p" },
        { do_is_rational, "is_rational", "i", "p" },
        { do_is_ref, "is_ref", "i", "p" },
        { do_is_semaphore, "is_semaphore", "i", "p" },
        { do_is_string, "is_string", "i", "p" },
        { do_is_struct, "is_struct", "i", "p" },
        { do_is_thread, "is_thread", "i", "p" },
        { do_is_void, "is_void", "i", "p" },
        { do_mantissa, "mantissa", "r", "R" },
        { do_numerator, "numerator", "i", "R" },
        { do_precision, "precision", "i", "R" },
        { do_profile, "profile", "i", "i" },
        { do_putchar, "putchar", "i", "i" },
        { do_reference, "reference", "*p", "p" },
        { do_sign, "sign", "i", "R" },
        { do_sleep, "sleep", "i", "i" },
        { do_string_to_real, "string_to_real", "R", "s" },
        { 0 }
    };

    static struct fbuiltin_2 funcs_2[] = {
        { do_gcd, "gcd", "i", "ii" },
        { do_setjmp, "setjmp", "p", "*cp" },
        { do_xor, "xor", "i", "ii" },
        { 0 }
    };

    static struct fbuiltin_2j funcs_2j[] = {
        { do_longjmp, "longjmp", "p", "cp" },
        { 0 }
    };

    static struct fbuiltin_v funcs_v[] = {
        { do_imprecise, "imprecise", "R", "R.i" },
        { do_string_to_integer, "string_to_integer", "i", "s.i" },
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
do_getchar ()
{
    ENTER ();
    RETURN (do_File_getc (FileStdin));
}

Value 
do_putchar (Value v)
{
    ENTER ();
    RETURN (do_File_putc (v, FileStdout));
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

extern Value	atov (char *, int);
extern Value	aetov (char *);

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
	RETURN(Zero);
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
    RETURN (aetov (StringChars (&str->string)));
}


Value
do_imprecise (int n, Value *p)
{
    ENTER();
    Value   v;
    int	    prec;

    v = p[0];
    if (n > 1)
	prec = IntPart (p[1], "imprecise: invalid precision");
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
	RETURN (Zero);
    exit (code);
    RETURN (Zero);
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
	RETURN (Zero);
    }
    ret = NewInt(av->array.dim[0]);
    RETURN (ret);
}

Value
do_dims(Value av) 
{
    ENTER();
    Value ret;
    int i;

    ret = NewArray(True, typesPrim[type_int], 1, &av->array.ndim);
    for (i = 0; i < av->array.ndim; i++) {
      Value d = NewInt(av->array.dim[i]);
      BoxValueSet(ret->array.values, i, d);
    }
    RETURN (ret);
}

Value
do_reference (Value av)
{
    ENTER ();
    Value   ret;

    ret = NewRef (NewBox (False, False, 1), 0);
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
	RETURN (Zero);
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
	RETURN (Zero);
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
    case type_int:
    case type_integer:
	break;
    case type_rational:
	av = NewInteger (av->rational.sign, av->rational.num);
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"numerator: argument must be precise",
				2, NewInt (0), av);
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_denominator (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_int:
    case type_integer:
	av = One;
	break;
    case type_rational:
	av = NewInteger (Positive, av->rational.den);
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"denominator: argument must be precise",
				2, NewInt (0), av);
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_bit_width (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_int:
	av = NewInt (IntWidth (av->ints.value));
	break;
    case type_integer:
	av = NewInt (NaturalWidth (av->integer.mag));
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"bit_width: argument must be integer",
				2, NewInt (0), av);
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_int (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_int:
    case type_integer:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_rational (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_int:
    case type_integer:
    case type_rational:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_number (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_int:
    case type_integer:
    case type_rational:
    case type_float:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_string (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_string:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_file (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_file:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_thread (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_thread:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_semaphore (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_semaphore:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_continuation (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_continuation:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_void (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_void:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_array (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_array:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_ref (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_ref:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_struct (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_struct:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_func (Value av)
{
    ENTER ();
    switch (ValueTag(av)) {
    case type_func:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}
