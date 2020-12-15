/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

Value	Blank;

int
StringInit (void)
{
    ENTER ();
    Blank = NewStrString("");
    MemAddRoot (Blank);
    EXIT ();
    return 1;
}

static Value
StringPlus (Value av, Value bv, int expandOk)
{
    ENTER();
    Value	ret;

    ret = NewString (av->string.length + bv->string.length);
    (void) memcpy (StringChars(&ret->string),
		   StringChars(&av->string),
		   av->string.length);
    (void) memcpy (StringChars(&ret->string) + av->string.length,
		   StringChars(&bv->string),
		   bv->string.length);
    RETURN (ret);
}

static Value
StringEqual (Value av, Value bv, int expandOk)
{
    if (av->string.length != bv->string.length)
	return FalseVal;
    if (!memcmp (StringChars (&av->string), 
		 StringChars(&bv->string),
		 av->string.length))
	return TrueVal;
    return FalseVal;
}

static Value
StringLess (Value av, Value bv, int expandOk)
{
    long    len;
    int	    c;

    len = av->string.length;
    if (len > bv->string.length)
	len = bv->string.length;
    c = memcmp (StringChars (&av->string), StringChars(&bv->string), len);
    if (c < 0)
	return TrueVal;
    if (c == 0 && av->string.length < bv->string.length)
	return TrueVal;
    return FalseVal;
}


static Bool
StringPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    char    *string = StringChars (&av->string);
    long    len = av->string.length;
    int	    print_width;
    
    print_width = FileStringWidth (string, len, format);
    while (width > print_width)
    {
	FileOutchar (f, fill);
	width--;
    }
    FilePutString (f, string, len, format);
    while (-width > print_width)
    {
	FileOutchar (f, fill);
	width++;
    }
    return True;
}

char *
StringNextChar (char *src, unsigned *dst, long *len)
{
    unsigned	result = *src++;
    
    if (!*len)
	return 0;
    (*len)--;
    
    if (result & 0x80)
    {
	int m = 0x20;
	int extra = 1;
	while (result & m)
	{
	    extra++;
	    m >>= 1;
	}
	result &= (m - 1);
	while (extra--)
	{
	    char c = *src++;

	    (*len)--;
	    if ((c & 0x80) == 0)
	    {
		src--;
		(*len)++;
		result = 0;
		break;
	    }
	    result = (result << 6) | (c & 0x3f);
	}
    }
    *dst = result;
    return src;
}

unsigned
StringGet (char *src, long len, int i)
{
    unsigned c;

    do
    {
	src = StringNextChar (src, &c, &len);
	if (!src)
	    return 0;
    } while (i-- > 0);
    return c;
}

int
StringLength (char *src, long len)
{
    int	l = 0;
    unsigned c;
    while ((src = StringNextChar (src, &c, &len)))
	l++;
    return l;
}

int
StringPutChar (unsigned c, char *dest)
{
    int	bits;
    char *d = dest;
    
         if (c <       0x80) { *d++=   c;                        bits= -6; }
    else if (c <      0x800) { *d++= ((c >>  6) & 0x1F) | 0xC0;  bits=  0; }
    else if (c <    0x10000) { *d++= ((c >> 12) & 0x0F) | 0xE0;  bits=  6; }
    else if (c <   0x200000) { *d++= ((c >> 18) & 0x07) | 0xF0;  bits= 12; }
    else if (c <  0x4000000) { *d++= ((c >> 24) & 0x03) | 0xF8;  bits= 18; }
    else if (c < 0x80000000) { *d++= ((c >> 30) & 0x01) | 0xFC;  bits= 24; }
    else return 0;

    for ( ; bits >= 0; bits-= 6) {
	*d++= ((c >> bits) & 0x3F) | 0x80;
    }
    return d - dest;
}

int
StringCharSize (unsigned c)
{
         if (c <       0x80) return 1;
    else if (c <      0x800) return 2;
    else if (c <    0x10000) return 3;
    else if (c <   0x200000) return 4;
    else if (c <  0x4000000) return 5;
    else if (c < 0x80000000) return 6;
    else return 0;
}

char *
StrzPart (Value v, char *error)
{
    if (!ValueIsString (v) || strlen (StringChars(&v->string)) != v->string.length)
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString (error),
				NewInt (0), v);
	return 0;
    }
    return StringChars (&v->string);
}

static HashValue
StringHash (Value av)
{
    return HashCrc32 ((unsigned char *) StringChars(&av->string),
		      av->string.length);
}

ValueRep   StringRep = {
    { 0, 0, "StringRep" },		/* base */
    rep_string,	/* tag */
    {			/* binary */
	StringPlus,
	0,
	0,
	0,
	0,
	0,
	StringLess,
	StringEqual,
	0,
	0,
    },
    {
	0,
    },
    0,
    0,
    StringPrint,
    0,
    StringHash,
};

Value
NewString (long length)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&StringRep.data, sizeof (String) + length + 1);
    ret->string.length = length;
    StringChars(&ret->string)[length] = '\0';
    RETURN (ret);
}

Value
NewStrString (const char *str)
{
    ENTER ();
    Value   ret;

    ret = NewString (strlen (str));
    strcpy (StringChars (&ret->string), str);
    RETURN (ret);
}

Value
NewCharString (int c)
{
    ENTER ();
    int	    size = StringCharSize (c);
    Value   ret = NewString (size);

    StringPutChar (c, StringChars (&ret->string));
    RETURN (ret);
}

