/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
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

    ret = NewString (strlen (StringChars(&av->string)) + 
		     strlen (StringChars(&bv->string)));
    (void) strcpy (StringChars(&ret->string),
		   StringChars(&av->string));
    (void) strcat (StringChars(&ret->string),
		   StringChars(&bv->string));
    RETURN (ret);
}

static Value
StringEqual (Value av, Value bv, int expandOk)
{
    if (!strcmp (StringChars (&av->string), StringChars(&bv->string)))
	return TrueVal;
    return FalseVal;
}

static Value
StringLess (Value av, Value bv, int expandOk)
{
    if (strcmp (StringChars (&av->string), StringChars(&bv->string)) < 0)
	return TrueVal;
    return FalseVal;
}


static Bool
StringPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    char    *string = StringChars (&av->string);
    int	    print_width;
    
    print_width = FileStringWidth (string, format);
    while (width > print_width)
    {
	FileOutchar (f, fill);
	width--;
    }
    FilePutString (f, string, format);
    while (-width > print_width)
    {
	FileOutchar (f, fill);
	width++;
    }
    return True;
}

char *
StringNextChar (char *src, unsigned *dst)
{
    unsigned	result = *src++;
    
    if (!result)
	return 0;
    
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

	    if ((c & 0x80) == 0)
	    {
		src--;
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
StringGet (char *src, int i)
{
    unsigned c;

    do
    {
	src = StringNextChar (src, &c);
	if (!src)
	    return 0;
    } while (i-- > 0);
    return c;
}

int
StringLength (char *src)
{
    int	len = 0;
    unsigned c;
    while ((src = StringNextChar (src, &c)))
	len++;
    return len;
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

#define hrot(i)	(((i) << 1) | ((i) >> (sizeof (HashValue) * 8 - 1)))

static HashValue
StringHash (Value av)
{
    char	*string = StringChars (&av->string);
    char	c;
    HashValue	h = 0;

    while ((c = *string++))
	h = hrot(h) ^ (HashValue) c;
    return h;
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
NewString (int len)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&StringRep.data, sizeof (String) + len + 1);
    RETURN (ret);
}

Value
NewStrString (char *str)
{
    ENTER ();
    Value   ret;

    ret = NewString (strlen (str));
    strcpy (StringChars (&ret->string), str);
    RETURN (ret);
}
