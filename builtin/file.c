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
    static const struct fbuiltin_0 funcs_0[] = {
        { do_File_string_write, "string_write", "f", "" },
        { 0 }
    };

    static const struct fbuiltin_1 funcs_1[] = {
        { do_File_clear_error, "clear_error", "v", "f" },
        { do_File_close, "close", "v", "f" },
        { do_File_end, "end", "b", "f" },
        { do_File_error, "error", "b", "f" },
        { do_File_flush, "flush", "v", "f" },
        { do_File_getb, "getb", "i", "f" },
	{ do_File_getc, "getc", "i", "f" },
        { do_File_string_read, "string_read", "f", "s" },
        { do_File_string_string, "string_string", "s", "f" },
	{ do_File_isatty, "isatty", "b", "f" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_File_open, "open", "f", "ss" },
        { do_File_putb, "putb", "i", "if" },
	{ do_File_putc, "putc", "i", "if" },
        { do_File_setbuf, "setbuffer", "i", "fi" },
        { do_File_ungetb, "ungetb", "i", "if" },
	{ do_File_ungetc, "ungetc", "i", "if" },
        { 0 }
    };

    static const struct fbuiltin_3 funcs_3[] = {
        { do_File_pipe, "pipe", "f", "sA*ss" },
	{ do_File_reopen, "reopen", "f", "ssf" },
        { 0 }
    };

    static const struct fbuiltin_7 funcs_7[] = {
        { do_File_print, "print", "v", "fpsiiis" },
        { 0 }
    };

    static const struct ebuiltin excepts[] = {
	{"open_error",		exception_open_error,		"sEs" },
	{"io_error",		exception_io_error,		"sEf" },
	{0,			0 },
    };

    const struct ebuiltin   *e;
    SymbolPtr		    s;

    FileNamespace = BuiltinNamespace (/*parent*/ 0, "File")->namespace.namespace;

    BuiltinFuncs0 (&FileNamespace, funcs_0);
    BuiltinFuncs1 (&FileNamespace, funcs_1);
    BuiltinFuncs2 (&FileNamespace, funcs_2);
    BuiltinFuncs3 (&FileNamespace, funcs_3);
    BuiltinFuncs7 (&FileNamespace, funcs_7);

    for (e = excepts; e->name; e++)
	BuiltinAddException (&FileNamespace, e->exception, e->name, e->args);

    s = NewSymbolType (AtomId("errorType"), typeFileError);
    NamespaceAddName (FileNamespace, s, publish_public);
    
    s = NewSymbolType (AtomId("error_type"), typeFileError);
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
	return Void;
    iwidth = IntPart (width, "Illegal width");
    if (aborting)
	return Void;
    iprec = IntPart (prec, "Illegal precision");
    if (aborting)
	return Void;
    if (file->file.flags & FileOutputBlocked)
	ThreadSleep (running, file, PriorityIo);
    else
    {
	Print (file, value, *StringChars(&format->string), ibase, iwidth,
	       iprec, *StringChars(&fill->string));
	if (file->file.flags & FileOutputError)
	{
	    RaiseStandardException (exception_io_error, 
				    FileGetErrorMessage (file->file.output_errno), 
				    2, FileGetError (file->file.output_errno), file);
	}
    }
    return Void;
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
	RETURN (Void);
    ret = FileFopen (n, m, &err);
    if (!ret)
    {
	RaiseStandardException (exception_open_error,
				FileGetErrorMessage (err),
				2, FileGetError (err), name);
	RETURN (Void);
    }
    complete = True;
    RETURN (ret);
}

Value 
do_File_flush (Value f)
{
    switch (FileFlush (f, False)) {
    case FileBlocked:
	ThreadSleep (running, f, PriorityIo);
	break;
    case FileError:
	RaiseStandardException (exception_io_error, 
				FileGetErrorMessage (f->file.output_errno), 
				2, FileGetError (f->file.output_errno), f);
	break;
    }
    return Void;
}

Value 
do_File_close (Value f)
{
    if (aborting)
	return Void;
    switch (FileFlush (f, False)) {
    case FileBlocked:
	ThreadSleep (running, f, PriorityIo);
	break;
    case FileError:
	RaiseStandardException (exception_io_error, 
				FileGetErrorMessage (f->file.output_errno), 
				2, FileGetError (f->file.output_errno), f);
	break;
    default:
	if (FileClose (f) == FileError)
	{
	    RaiseStandardException (exception_io_error, 
				    FileGetErrorMessage (f->file.output_errno), 
				    2, FileGetError (f->file.output_errno), f);
	}
	else
	    complete = True;
    }
    return Void;
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

    args = AllocateTemp ((ArrayDims(&argv->array)[0] + 1) * sizeof (char *));
    for (argc = 0; argc < ArrayDims(&argv->array)[0]; argc++)
    {
	arg = BoxValue (argv->array.values, argc);
	args[argc] = StringChars (&arg->string);
    }
    args[argc] = 0;
    if (aborting)
	RETURN(Void);
    ret = FilePopen (StringChars (&file->string), args, 
		     StringChars (&mode->string), &err);
    if (!ret)
    {
	RaiseStandardException (exception_open_error,
				FileGetErrorMessage (err),
				2, FileGetError (err), file);
	ret = Void;
    }
    complete = True;
    RETURN (ret);
}

Value
do_File_reopen (Value name, Value mode, Value file)
{
    ENTER ();
    char	*n, *m;
    Value	ret;
    int		err;

    n = StringChars (&name->string);
    m = StringChars (&mode->string);
    if (aborting)
	RETURN (Void);
    ret = FileReopen (n, m, file, &err);
    if (!ret)
    {
	RaiseStandardException (exception_open_error,
				FileGetErrorMessage (err),
				2, FileGetError (err), name);
	RETURN (Void);
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
do_File_isatty (Value file)
{
    ENTER ();
    if (file->file.flags & FileString)
	return FalseVal;
    RETURN(isatty (file->file.fd) ? TrueVal : FalseVal);
}

Value 
do_File_getb (Value f)
{
    ENTER ();
    int	    c;
    
    if (!aborting)
    {
	c = FileInput (f);
	switch (c) {
	case FileBlocked:
	    ThreadSleep (running, f, PriorityIo);
	    RETURN (Void);
	case FileError:
	    RaiseStandardException (exception_io_error,
				    FileGetErrorMessage (f->file.input_errno),
				    2, FileGetError (f->file.input_errno), f);
	    RETURN (Void);
	default:
	    complete = True;
	    RETURN (NewInt (c));
	}
    }
    RETURN (Void);
}

Value 
do_File_getc (Value f)
{
    ENTER ();
    int	    c;
    
    if (!aborting)
    {
	c = FileInchar (f);
	switch (c) {
	case FileBlocked:
	    ThreadSleep (running, f, PriorityIo);
	    RETURN (Void);
	case FileError:
	    RaiseStandardException (exception_io_error,
				    FileGetErrorMessage (f->file.input_errno),
				    2, FileGetError (f->file.input_errno), f);
	    RETURN (Void);
	default:
	    complete = True;
	    RETURN (NewInt (c));
	}
    }
    RETURN (Void);
}

Value
do_File_end (Value f)
{
    ENTER ();
    if (f->file.flags & FileEnd)
	RETURN (TrueVal);
    else
	RETURN (FalseVal);
}

Value
do_File_error (Value f)
{
    ENTER ();
    if (f->file.flags & (FileInputError|FileOutputError))
	RETURN (TrueVal);
    else
	RETURN (FalseVal);
}

Value
do_File_clear_error (Value f)
{
    ENTER ();
    f->file.flags &= ~(FileInputError|FileOutputError|FileEnd);
    RETURN (Void);
}

Value 
do_File_putb (Value v, Value f)
{
    ENTER ();
    
    if (f->file.flags & FileOutputBlocked)
	ThreadSleep (running, f, PriorityIo);
    else
    {
	if (!aborting)
	{
	    if (FileOutput (f, IntPart (v, "putb non integer")) == FileError)
	    {
		RaiseStandardException (exception_io_error,
					FileGetErrorMessage (f->file.output_errno),
					2, FileGetError (f->file.output_errno), f);
	    }
	    else
		complete = True;
	}
    }
    RETURN (v);
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
	    if (FileOutchar (f, IntPart (v, "putc non integer")) == FileError)
	    {
		RaiseStandardException (exception_io_error,
					FileGetErrorMessage (f->file.output_errno),
					2, FileGetError (f->file.output_errno), f);
	    }
	    else
		complete = True;
	}
    }
    RETURN (v);
}

Value 
do_File_ungetb (Value v, Value f)
{
    ENTER ();
    
    if (f->file.flags & FileOutputBlocked)
	ThreadSleep (running, f, PriorityIo);
    else
    {
	if (!aborting)
	{
	    complete = True;
	    FileUnput (f, IntPart (v, "ungetb: non integer"));
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
	    FileUnchar (f, IntPart (v, "ungetc: non integer"));
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
