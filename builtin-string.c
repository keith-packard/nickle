/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	string.c
 *
 *	provide builtin functions for the String namespace
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr StringNamespace;

void
import_String_namespace()
{
    ENTER ();
    static struct fbuiltin_1 funcs_1[] = {
        { do_String_length, "length", "i", "s" },
        { do_String_new, "new", "s", "p" },
        { 0 }
    };

    static struct fbuiltin_2 funcs_2[] = {
        { do_String_index, "index", "i", "ss" },
        { 0 }
    };

    static struct fbuiltin_3 funcs_3[] = {
        { do_String_substr, "substr", "s", "sii" },
        { 0 }
    };

    StringNamespace = BuiltinNamespace (/*parent*/ 0, "String")->namespace.namespace;

    BuiltinFuncs1 (&StringNamespace, funcs_1);
    BuiltinFuncs2 (&StringNamespace, funcs_2);
    BuiltinFuncs3 (&StringNamespace, funcs_3);
    EXIT ();
}

#endif

Value
do_String_length (Value av)
{
    ENTER();
    Value ret;
    ret = NewInt(strlen(StringChars(&av->string)));
    RETURN (ret);
}

Value
do_String_new (Value av)
{
    ENTER ();
    Value   ret;
    int	    len, i;
    char    *s;

    if (av->value.tag == type_array && av->array.ndim == 1)
    {
	len = av->array.dim[0];
	ret = NewString (len);
	s = StringChars (&ret->string);
	for (i = 0; i < len; i++)
	    *s++ = IntPart (BoxValue (av->array.values, i),
			    "new: array element not integer");
	*s++ = '\0';
    }
    else
    {
	len = 1;
	ret = NewString (len);
	s = StringChars (&ret->string);
	s[0] = IntPart (av, "new: argument not integer");
	s[1] = '\0';
    }
    RETURN (ret);
}
	       
Value
do_String_index (Value av, Value bv)
{
    ENTER();
    char *a, *b, *p;
    Value ret;
    a = StringChars(&av->string);
    b = StringChars(&bv->string);
    p = strstr(a, b);
    if (!p)
	RETURN (NewInt(-1));
    ret = NewInt(p - a);
    RETURN (ret);
}

Value
do_String_substr (Value av, Value bv, Value cv)
{
    ENTER();
    char *a, *rchars;
    int b, c, al;
    Value ret;
    a = StringChars(&av->string);
    al = strlen(a);
    b = IntPart(bv, "substr: index not integer");
    c = IntPart(cv, "substr: count not integer");
    if (c < 0) {
	b += c;
	c = -c;
    }
    if (b < 0 || b >= al)
    {
	RaiseStandardException (exception_invalid_argument,
				"substr: index out of range",
				2, NewInt (1), bv);
	RETURN (av);
    }
    if (b + c > al)
    {
	RaiseStandardException (exception_invalid_argument,
				"substr: count out of range",
				2, NewInt (2), cv);
	RETURN (av);
    }
    ret = NewString(c);
    rchars = StringChars(&ret->string);
    strncpy(rchars, a + b, c);
    rchars[c] = '\0';
    RETURN (ret);
}
