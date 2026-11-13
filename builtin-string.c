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

#include <ctype.h>
#include <strings.h>
#include <time.h>
#include "builtin.h"

NamespacePtr StringNamespace;

void
import_String_namespace(void)
{
    ENTER();
    static const struct fbuiltin_1 funcs_1[] = {
        { do_String_length, "length", "i", "s",
         "\n"
          " int length (string s)\n"
          "\n"
          " Return the number of characters in 's'.\n" },
        { do_String_new, "new", "s", "p",
         "\n"
          " string new (int c)\n"
          " string new (int[*] a)\n"
          "\n"
          " With 'c', return a single character string containing it.\n"
          " With 'a', return a string constructed from the list of\n"
          " characters in 'a'.\n" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_String_index, "index", "i", "ss",
         "\n"
          " int index (string str, string substr)\n"
          "\n"
          " Return the index of the first location of 'substr'\n"
          " within 'str', -1 if not found.\n" },
        { 0 }
    };

    static const struct fbuiltin_3 funcs_3[] = {
        { do_String_substr, "substr", "s", "sii",
         "\n"
          " string substr (string str, int first, int len)\n"
          "\n"
          " Return a string containing characters in 'str' starting at\n"
          " 'first' for 'len' characters.\n" },
        { 0 }
    };

    StringNamespace = BuiltinNamespace(/*parent*/ 0, "String")->namespace.namespace;

    BuiltinFuncs1(&StringNamespace, funcs_1);
    BuiltinFuncs2(&StringNamespace, funcs_2);
    BuiltinFuncs3(&StringNamespace, funcs_3);
    EXIT();
}

Value
do_String_length(Value av)
{
    ENTER();
    Value ret;
    ret = NewPtrdiffInteger(StringLength(StringChars(&av->string), av->string.length));
    RETURN(ret);
}

Value
do_String_new(Value av)
{
    ENTER();
    Value     ret;
    ptrdiff_t len, i;
    ptrdiff_t size;
    char     *s;

    if (ValueIsArray(av) && av->array.ndim == 1) {
        len = ArrayLimits(&av->array)[0];
        size = 0;
        for (i = 0; i < len; i++)
            size += StringCharSize(
                CharPart(ArrayValue(&av->array, i), "new: array element not a character"));
        ret = NewString(size);
        s = StringChars(&ret->string);
        for (i = 0; i < len; i++) {
            s += StringPutChar(
                CharPart(ArrayValue(&av->array, i), "new: array element not a character"), s);
        }
    } else {
        uint32_t c = CharPart(av, "new: argument not a character");
        size = StringCharSize(c);
        ret = NewString(size);
        s = StringChars(&ret->string);
        s += StringPutChar(c, s);
    }
    RETURN(ret);
}

Value
do_String_index(Value av, Value bv)
{
    ENTER();
    char     *a, *b, *p;
    ptrdiff_t al;
    Value     ret;
    ptrdiff_t i;
    a = StringChars(&av->string);
    al = av->string.length;
    b = StringChars(&bv->string);
    /* Strings may hold NUL, so compare lengths rather than C strings */
    p = memmem(a, (size_t)al, b, (size_t)bv->string.length);
    if (!p)
        RETURN(NewInt(-1));
    i = 0;
    while (a < p) {
        uint32_t c;
        a = StringNextChar(a, &c, &al);
        i++;
    }
    ret = NewPtrdiffInteger(i);
    RETURN(ret);
}

Value
do_String_substr(Value av, Value bv, Value cv)
{
    ENTER();
    char     *a, *rchars, *e;
    ptrdiff_t b, c, al, size;
    Value     ret;
    ptrdiff_t alen = av->string.length;
    ptrdiff_t elen;

    a = StringChars(&av->string);
    al = StringLength(a, alen);
    /*
     * Check the index before using it, so that none of the arithmetic
     * below can overflow. A negative count ends the substring at the
     * index.
     */
    if (!ValueToPtrdiff(bv, &b) || b < 0 || b > al) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("substr: index out of range"), NewInt(1), bv);
        RETURN(av);
    }
    if (!ValueToPtrdiff(cv, &c) || (c < 0 && c < -b)) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("substr: count out of range"), NewInt(2), cv);
        RETURN(av);
    }
    if (c < 0) {
        b += c;
        c = -c;
    }
    if (c > al - b) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("substr: count out of range"), NewInt(2), cv);
        RETURN(av);
    }
    /*
     * Find start of substring
     */
    while (b > 0) {
        uint32_t ch;
        a = StringNextChar(a, &ch, &alen);
        b--;
    }
    /*
     * Find size of substring
     */
    e = a;
    elen = alen;
    while (c > 0) {
        uint32_t ch;
        e = StringNextChar(e, &ch, &elen);
        c--;
    }
    size = e - a;
    ret = NewString(size);
    rchars = StringChars(&ret->string);
    memcpy(rchars, a, (size_t)size);
    RETURN(ret);
}
