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

struct ebuiltin {
    char		*name;
    StandardException	exception;
    char		*args;
};


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

    static struct ebuiltin excepts[] = {
	{"open_error",		exception_open_error,		"sEs" },
	{"io_error",		exception_io_error,		"sEf" },
	{0,			0 },
    };

    struct ebuiltin *e;
    SymbolPtr	    s;

    FileNamespace = BuiltinNamespace (/*parent*/ 0, "File")->namespace.namespace;

    BuiltinFuncs0 (&FileNamespace, funcs_0);
    BuiltinFuncs1 (&FileNamespace, funcs_1);
    BuiltinFuncs2 (&FileNamespace, funcs_2);
    BuiltinFuncs3 (&FileNamespace, funcs_3);
    BuiltinFuncs7 (&FileNamespace, funcs_7);

    for (e = excepts; e->name; e++)
	BuiltinAddException (&FileNamespace, e->exception, e->name, e->args);

    s = NewSymbolType (AtomId("errorType"), typesFileError);
    NamespaceAddName (FileNamespace, s, publish_public);
    
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
    {
	Print (file, value, *StringChars(&format->string), ibase, iwidth,
	       iprec, *StringChars(&fill->string));
	if (file->file.flags & FileOutputError)
	{
	    RaiseStandardException (exception_io_error, 
				    strerror (file->file.output_errno), 
				    2, FileGetError (file->file.output_errno), file);
	}
    }
    return Zero;
}

Value 
do_File_open (Value name, Value mode)
{
    ENTER ();
    char	*n, *m;
    Value	ret;
    int		err;

    n = StringChars (&name->string);
    m = StringChars (&mode->string);
    if (aborting)
	RETURN (Zero);
    ret = FileFopen (n, m, &err);
    if (!ret)
    {
	RaiseStandardException (exception_open_error,
				strerror (err),
				2, FileGetError (err), name);
	RETURN (Zero);
    }
    complete = True;
    RETURN (ret);
}

Value 
do_File_flush (Value f)
{
    switch (FileFlush (f)) {
    case FileBlocked:
	ThreadSleep (running, f, PriorityIo);
	break;
    case FileError:
	RaiseStandardException (exception_io_error, 
				strerror (f->file.output_errno), 
				2, FileGetError (f->file.output_errno), f);
	break;
    }
    return One;
}

Value 
do_File_close (Value f)
{
    if (aborting)
	return Zero;
    switch (FileFlush (f)) {
    case FileBlocked:
	ThreadSleep (running, f, PriorityIo);
	break;
    case FileError:
	RaiseStandardException (exception_io_error, 
				strerror (f->file.output_errno), 
				2, FileGetError (f->file.output_errno), f);
	break;
    default:
	if (FileClose (f) == FileError)
	{
	    RaiseStandardException (exception_io_error, 
				    strerror (f->file.output_errno), 
				    2, FileGetError (f->file.output_errno), f);
	}
	else
	    complete = True;
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
    Value   ret;
    int	    err;

    args = AllocateTemp ((argv->array.dim[0] + 1) * sizeof (char *));
    for (argc = 0; argc < argv->array.dim[0]; argc++)
    {
	arg = BoxValue (argv->array.values, argc);
	args[argc] = StringChars (&arg->string);
    }
    args[argc] = 0;
    if (aborting)
	RETURN(Zero);
    ret = FilePopen (StringChars (&file->string), args, 
		     StringChars (&mode->string), &err);
    if (!ret)
    {
	RaiseStandardException (exception_open_error,
				strerror (err),
				2, FileGetError (err), file);
	ret = Zero;
    }
    complete = True;
    RETURN (ret);
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
	switch (c) {
	case FileBlocked:
	    ThreadSleep (running, f, PriorityIo);
	    RETURN (Zero);
	case FileError:
	    RaiseStandardException (exception_io_error,
				    strerror (f->file.input_errno),
				    2, FileGetError (f->file.input_errno), f);
	    RETURN (Zero);
	default:
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
	    if (FileOutput (f, IntPart (v, "putc non integer")) == FileError)
	    {
		RaiseStandardException (exception_io_error,
					strerror (f->file.output_errno),
					2, FileGetError (f->file.output_errno), f);
	    }
	    else
		complete = True;
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
