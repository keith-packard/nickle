/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
    static const struct fbuiltin_1 funcs_1[] = {
        { do_String_length, "length", "i", "s", "\n"
	    " int length (string s)\n"
	    "\n"
	    " Return the number of characters in 's'.\n" },
        { do_String_new, "new", "s", "p", "\n"
	    " string new (int c)\n"
	    " string new (int[*] a)\n"
	    "\n"
	    " With 'c', return a single character string containing it.\n"
	    " With 'a', return a string constructed from the list of\n"
	    " characters in 'a'.\n" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_String_index, "index", "i", "ss", "\n"
	    " int index (string str, string substr)\n"
	    "\n"
	    " Return the index of the first location of 'substr'\n"
	    " within 'str', -1 if not found.\n" },
        { 0 }
    };

    static const struct fbuiltin_3 funcs_3[] = {
        { do_String_substr, "substr", "s", "sii", "\n" 
	    " string substr (string str, int first, int len)\n"
	    "\n"
	    " Return a string containing characters in 'str' starting at\n"
	    " 'first' for 'len' characters.\n" },
        { 0 }
    };

    StringNamespace = BuiltinNamespace (/*parent*/ 0, "String")->namespace.namespace;

    BuiltinFuncs1 (&StringNamespace, funcs_1);
    BuiltinFuncs2 (&StringNamespace, funcs_2);
    BuiltinFuncs3 (&StringNamespace, funcs_3);
    EXIT ();
}

Value
do_String_length (Value av)
{
    ENTER();
    Value ret;
    ret = NewInt(StringLength(StringChars(&av->string), av->string.length));
    RETURN (ret);
}

Value
do_String_new (Value av)
{
    ENTER ();
    Value   ret;
    int	    len, i, size;
    char    *s;

    if (ValueIsArray(av) && av->array.ndim == 1)
    {
	len = ArrayLimits(&av->array)[0];
	size = 0;
	for (i = 0; i < len; i++)
	    size += StringCharSize (IntPart (ArrayValue (&av->array, i),
					     "new: array element not integer"));
	ret = NewString (size);
	s = StringChars (&ret->string);
	for (i = 0; i < len; i++)
	{
	    s += StringPutChar (IntPart (ArrayValue (&av->array, i),
					 "new: array element not integer"),
				s);
	}
    }
    else
    {
	int c = IntPart (av, "new: argument not integer");
	size = StringCharSize (c);
	ret = NewString (size);
	s = StringChars (&ret->string);
	s += StringPutChar (c, s);
    }
    RETURN (ret);
}
	       
Value
do_String_index (Value av, Value bv)
{
    ENTER();
    char *a, *b, *p;
    long al;
    Value ret;
    int i;
    a = StringChars(&av->string);
    al = av->string.length;
    b = StringChars(&bv->string);
    p = strstr(a, b);
    if (!p)
	RETURN (NewInt(-1));
    i = 0;
    while (a < p)
    {
	unsigned c;
	a = StringNextChar (a, &c, &al);
	i++;
    }
    ret = NewInt(i);
    RETURN (ret);
}

Value
do_String_substr (Value av, Value bv, Value cv)
{
    ENTER();
    char *a, *rchars, *e;
    int b, c, al, size;
    Value ret;
    long alen = av->string.length;
    long elen;

    a = StringChars(&av->string);
    al = StringLength (a, alen);
    b = IntPart(bv, "substr: index not integer");
    c = IntPart(cv, "substr: count not integer");
    if (c < 0) {
	b += c;
	c = -c;
    }
    if (b < 0 || b > al)
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("substr: index out of range"),
				NewInt (1), bv);
	RETURN (av);
    }
    if (b + c > al)
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("substr: count out of range"),
				NewInt (2), cv);
	RETURN (av);
    }
    /*
     * Find start of substring
     */
    while (b > 0)
    {
	unsigned ch;
	a = StringNextChar (a, &ch, &alen);
	b--;
    }
    /*
     * Find size of substring
     */
    e = a;
    elen = alen;
    while (c > 0)
    {
	unsigned ch;
	e = StringNextChar (e, &ch, &elen);
	c--;
    }
    size = e - a;
    ret = NewString(size);
    rchars = StringChars(&ret->string);
    memcpy (rchars, a, size);
    RETURN (ret);
}
