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
	return One;
    return Zero;
}

static Value
StringLess (Value av, Value bv, int expandOk)
{
    if (strcmp (StringChars (&av->string), StringChars(&bv->string)) < 0)
	return One;
    return Zero;
}

static int
printWidth (char *s)
{
    int	    width = 2;
    char    c;
    while ((c = *s++))
    {
	if (c < ' ' || '~' < c)
	    switch (c) {
	    case '\n':
	    case '\r':
	    case '\t':
	    case '\b':
		width += 2;
		break;
	    default:
		width += 4;
		break;
	    }
	else
	    width++;
    }
    return width;
}

static Bool
StringPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    char    *string = StringChars (&av->string);
    char    c;
    int	    print_width;
    
    if (format == 's')
	print_width = strlen (string);
    else
	print_width = printWidth (string);
    while (width > print_width)
    {
	FileOutput (f, fill);
	width--;
    }
    if (format == 's')
	FilePuts (f, string);
    else
    {
	FileOutput (f, '"');
	while ((c = *string++)) {
	    if (c < ' ' || '~' < c)
		switch (c) {
		case '\n':
		    FilePuts (f, "\\n");
		    break;
		case '\r':
		    FilePuts (f, "\\r");
		    break;
		case '\t':
		    FilePuts (f, "\\t");
		    break;
		case '\b':
		    FilePuts (f, "\\b");
		    break;
		default:
		    FileOutput (f, '\\');
		    Print (f, NewInt (c), 'o', 8, 3, 0, '0');
		    break;
		}
	    else
		FileOutput (f, c);
	}
	FileOutput (f, '"');
    }
    while (-width > print_width)
    {
	FileOutput (f, fill);
	width++;
    }
    return True;
}

ValueType   StringType = {
    { 0, 0 },
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
};

Value
NewString (int len)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&StringType.data, sizeof (String) + len + 1);
    ret->value.tag = type_string;
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
