/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	file.c
 *
 *	provide builtin functions for the File namespace
 */

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr FileNamespace;

void
import_File_namespace()
{
    ENTER ();
    static struct fbuiltin_0 funcs_0[] = {
        { do_File_string_write, "string_write", "f", "" },
        { 0 }
    };

    static struct fbuiltin_1 funcs_1[] = {
        { do_File_clear_error, "clear_error", "i", "f" },
        { do_File_close, "close", "i", "f" },
        { do_File_end, "end", "i", "f" },
        { do_File_error, "error", "i", "f" },
        { do_File_flush, "flush", "i", "f" },
        { do_File_getc, "getc", "i", "f" },
        { do_File_string_read, "string_read", "f", "s" },
        { do_File_string_string, "string_string", "s", "f" },
        { 0 }
    };

    static struct fbuiltin_2 funcs_2[] = {
        { do_File_open, "open", "f", "ss" },
        { do_File_putc, "putc", "i", "if" },
        { do_File_setbuf, "setbuffer", "i", "fi" },
        { do_File_ungetc, "ungetc", "i", "if" },
        { 0 }
    };

    static struct fbuiltin_3 funcs_3[] = {
        { do_File_pipe, "pipe", "f", "sA*ss" },
        { do_File_vfprintf, "vfprintf", "i", "fsA*p" },
        { 0 }
    };

    static struct fbuiltin_7 funcs_7[] = {
        { do_File_print, "print", "i", "fpsiiis" },
        { 0 }
    };

    static struct fbuiltin_v funcs_v[] = {
        { do_fprintf, "fprintf", "i", "fs.p" },
        { 0 }
    };

    FileNamespace = BuiltinNamespace (/*parent*/ 0, "File")->namespace.namespace;

    BuiltinFuncs0 (&FileNamespace, funcs_0);
    BuiltinFuncs1 (&FileNamespace, funcs_1);
    BuiltinFuncs2 (&FileNamespace, funcs_2);
    BuiltinFuncs3 (&FileNamespace, funcs_3);
    BuiltinFuncs7 (&FileNamespace, funcs_7);
    BuiltinFuncsV (&FileNamespace, funcs_v);
    EXIT ();
}

int
dofformat (Value f, char *fmt, int n, Value *p)
{
    int	    pn, npn;
    int	    base;
    int	    width, prec, sign;
    char    fill;
    char    c;
    char    *fm;
    char    *e;
    
    pn = 0;
    for (fm = fmt; *(e = fm); fm++)
    {
	npn = pn;
	if ((c = *fm) == '%') 
	{
	    fill = ' ';
	    width = 0;
	    prec = DEFAULT_OUTPUT_PRECISION;
	    sign = 1;
	    switch (c = *++fm) {
	    case '%':
		FileOutput (f, c);
		break;
	    case '*':
		if (pn >= n)
		{
		    RaiseStandardException (exception_invalid_argument,
					    "Too few arguments to print",
					    2,
					    NewInt (pn),
					    NewInt (n));
		    return -1;
		}
		width = IntPart (p[pn], "Illegal format width");
		if (aborting)
		    return -1;
		++pn;
		c = *++fm;
		goto dot;
	    case '-':
		sign = -1;
		if ((c = *++fm) == '0')
	    case '0':
		    fill = '0';
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		width = c - '0';
		while (isdigit ((int)(c = *++fm)))
		    width = width * 10 + c - '0';
		width = width * sign;
	    dot:
		if (c == '.')
		{
	    case '.':
		    switch (c = *++fm)
		    {
		    case '-':
			prec = INFINITE_OUTPUT_PRECISION;
			c = *++fm;
			break;
		    case '*':
			if (pn >= n)
			{
			    RaiseStandardException (exception_invalid_argument,
						    "Too few arguments to print",
						    2,
						    NewInt (pn),
						    NewInt (n));
			    return -1;
			}
			prec = IntPart (p[pn], "Illegal format precision");
			if (aborting)
			    return -1;
			++pn;
			c = *++fm;
			break;
		    case '0':
		    case '1':
		    case '2':
		    case '3':
		    case '4':
		    case '5':
		    case '6':
		    case '7':
		    case '8':
		    case '9':
			prec = c - '0';
			while (isdigit ((int)(c = *++fm)))
			    prec = prec * 10 + c - '0';
			break;
		    }
		}
	    default:
		base = 0;
		switch (c) {
		case 'b': base = 2; break;
		case 'x': base = 16; break;
		case 'X': base = 16; break;
		case 'o': base = 8; break;
		}
		if (n > pn) {
		    if (!Print (f, p[pn], c, base, width, prec, fill))
			return (e - fmt) | (pn << 16);
		    pn++;
		} else {
		    RaiseStandardException (exception_invalid_argument,
					    "Too few arguments to print",
					    2,
					    NewInt (pn),
					    NewInt (n));
		    return -1;
		}
		break;
	    }
	}
	else
	    FileOutput (f, c);
    }
    if (pn != n)
	RaiseStandardException (exception_invalid_argument,
				"Too many arguments to print",
				2,
				NewInt (pn),
				NewInt (n));
    return -1;
}

void
callformat (Value f, char *fmt, int n, Value *p)
{
    int	    r, nr;

    if (f->file.flags & FileOutputBlocked)
	ThreadSleep (running, f, PriorityIo);
    else
    {
	r = running->thread.partial;
	if (r)
	{
	    fmt += r & 0xffff;
	    p += (r >> 16);
	}
	nr = dofformat (f, fmt, n, p);
	if (nr >= 0)
	    running->thread.partial += nr;
	else if (!aborting)	/* only complete if no errors */
	    complete = True;
    }
}

Value 
do_fprintf (int n, Value *p)
{
    Value	f;
    char	*fmt;
    
    f = p[0];
    p++;
    n--;
    fmt = StringChars(&p[0]->string);
    p++;
    n--;
    callformat (f, fmt, n, p);
    return Zero;
}

Value
do_File_vfprintf (Value file, Value format, Value args)
{
    ENTER ();
    Value   *a;
    int	    i;
    char    *fmt = StringChars (&format->string);
    
    a = AllocateTemp (args->array.ents * sizeof (Value));
    for (i = 0; i < args->array.ents; i++)
    {
	a[i] = BoxValue (args->array.values, i);
	if (aborting)
	    RETURN (Zero);
    }
    callformat (file, fmt, args->array.ents, a);
    RETURN (Zero);
}

Value
do_File_print (Value file, Value value, Value format, 
	       Value base, Value width, Value prec, Value fill)
{
    int	    ibase, iwidth, iprec;
    
    ibase = IntPart (base, "Illegal base");
    if (aborting)
	return Zero;
    iwidth = IntPart (width, "Illegal width");
    if (aborting)
	return Zero;
    iprec = IntPart (prec, "Illegal precision");
    if (aborting)
	return Zero;
    if (file->file.flags & FileOutputBlocked)
	ThreadSleep (running, file, PriorityIo);
    else
	Print (file, value, *StringChars(&format->string), ibase, iwidth,
	       iprec, *StringChars(&fill->string));
    return Zero;
}

Value 
do_File_open (Value name, Value mode)
{
    ENTER ();
    char	*n, *m;
    Value	ret;

    n = StringChars (&name->string);
    m = StringChars (&mode->string);
    if (aborting)
	RETURN (Zero);
    complete = True;
    ret = FileFopen (n, m);
    if (!ret)
	RETURN (Zero);
    RETURN (ret);
}

Value 
do_File_flush (Value f)
{
    if (FileFlush (f) == FileBlocked)
	ThreadSleep (running, f, PriorityIo);
    return One;
}

Value 
do_File_close (Value f)
{
    if (aborting)
	return Zero;
    if (FileFlush (f) == FileBlocked)
	ThreadSleep (running, f, PriorityIo);
    else
    {
	complete = True;
	(void) FileClose (f);
    }
    return One;
}

Value
do_File_pipe (Value file, Value argv, Value mode)
{
    ENTER ();
    char    **args;
    int	    argc;
    Value   arg;

    args = AllocateTemp ((argv->array.dim[0] + 1) * sizeof (char *));
    for (argc = 0; argc < argv->array.dim[0]; argc++)
    {
	arg = BoxValue (argv->array.values, argc);
	args[argc] = StringChars (&arg->string);
    }
    args[argc] = 0;
    RETURN (FilePopen (StringChars (&file->string), args, 
		      StringChars (&mode->string)));
}

Value
do_File_string_read (Value s)
{
    ENTER ();
    char    *str;

    str = StringChars (&s->string);
    RETURN (FileStringRead (str, strlen (str)));
}

Value
do_File_string_write (void)
{
    ENTER ();
    RETURN (FileStringWrite ());
}

Value
do_File_string_string (Value f)
{
    ENTER ();
    RETURN (FileStringString (f));
}

Value 
do_File_getc (Value f)
{
    ENTER ();
    int	    c;
    
    if (!aborting)
    {
	c = FileInput (f);
	if (c == FileBlocked)
	{
	    ThreadSleep (running, f, PriorityIo);
	    RETURN (Zero);
	}
	else
	{
	    complete = True;
	    RETURN (NewInt (c));
	}
    }
    RETURN (Zero);
}

Value
do_File_end (Value f)
{
    ENTER ();
    if (f->file.flags & FileEnd)
	RETURN (One);
    else
	RETURN (Zero);
}

Value
do_File_error (Value f)
{
    ENTER ();
    if (f->file.flags & (FileInputError|FileOutputError))
	RETURN (One);
    else
	RETURN (Zero);
}

Value
do_File_clear_error (Value f)
{
    ENTER ();
    f->file.flags &= ~(FileInputError|FileOutputError|FileEnd);
    RETURN (One);
}

Value 
do_File_putc (Value v, Value f)
{
    ENTER ();
    
    if (f->file.flags & FileOutputBlocked)
	ThreadSleep (running, f, PriorityIo);
    else
    {
	if (!aborting)
	{
	    complete = True;
	    FileOutput (f, IntPart (v, "putc non integer"));
	}
    }
    RETURN (v);
}

Value 
do_File_ungetc (Value v, Value f)
{
    ENTER ();
    
    if (f->file.flags & FileOutputBlocked)
	ThreadSleep (running, f, PriorityIo);
    else
    {
	if (!aborting)
	{
	    complete = True;
	    FileUnput (f, IntPart (v, "ungetc: non integer"));
	}
    }
    RETURN (v);
}

Value
do_File_setbuf (Value f, Value v)
{
    ENTER ();
    int	i;

    i = IntPart (v, "setbuffer non integer");
    if (!aborting)
	FileSetBuffer (f, i);
    RETURN (v);
}
