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


static Bool
StringPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    char    *string = StringChars (&av->string);
    int	    print_width;
    
    print_width = FileStringWidth (string, format);
    while (width > print_width)
    {
	FileOutput (f, fill);
	width--;
    }
    FilePutString (f, string, format);
    while (-width > print_width)
    {
	FileOutput (f, fill);
	width++;
    }
    return True;
}

ValueType   StringType = {
    { 0, 0 },		/* base */
    type_string,	/* tag */
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
