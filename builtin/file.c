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
        { 0 }
    };

    static struct fbuiltin_7 funcs_7[] = {
        { do_File_print, "print", "i", "fpsiiis" },
        { 0 }
    };

    FileNamespace = BuiltinNamespace (/*parent*/ 0, "File")->namespace.namespace;

    BuiltinFuncs0 (&FileNamespace, funcs_0);
    BuiltinFuncs1 (&FileNamespace, funcs_1);
    BuiltinFuncs2 (&FileNamespace, funcs_2);
    BuiltinFuncs3 (&FileNamespace, funcs_3);
    BuiltinFuncs7 (&FileNamespace, funcs_7);
    EXIT ();
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
