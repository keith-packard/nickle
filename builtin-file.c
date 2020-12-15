/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
#include	<errno.h>
#include	<sys/stat.h>
#include	<sys/types.h>
#include	"builtin.h"

NamespacePtr FileNamespace;

void
import_File_namespace()
{
    ENTER ();
    static const struct fbuiltin_0 funcs_0[] = {
        { do_File_string_write, "string_write", "f", "", "\n"
	    " file string_write ()\n"
	    "\n"
	    " Returns a writable file which can be converted to\n"
	    " a string with string_string.\n"
	},
	{ do_File_mkpipe, "mkpipe", "A*f", "", "\n"
	    " file[*] mkpipe ()\n"
	    "\n"
	    " Returns an array of two file objects representing\n"
	    " the endpoints of a pipe.\n"
	    " Data written to [1] will be available for reading on [0].\n" },
        { 0 }
    };

    static const struct fbuiltin_1 funcs_1[] = {
        { do_File_clear_error, "clear_error", "v", "f", "\n"
	    " void clear_error (file f)\n"
	    "\n"
	    " Clear any error or end-of-file condition on 'f'.\n" },
        { do_File_close, "close", "v", "f", "\n"
	    " void close (file f)\n"
	    "\n"
	    " Close 'f'.  Note that files will automatically be closed\n"
	    " at some point after they become unreachable.  This function\n"
	    " simply provides a mechanism for causing it to happen at\n"
	    " a specific point in time.\n" },
        { do_File_end, "end", "b", "f", "\n"
	    " bool end (file f)\n"
	    "\n"
	    " Returns true if 'f' is at the end of file.\n"
	    " This often entails attempting to read a character from 'f'\n"
	    " if the end has not already been discovered.\n" },
        { do_File_error, "error", "b", "f", "\n"
	    " bool error (file f)\n"
	    "\n"
	    " Returns true if 'f' has an error condition set.\n" },
        { do_File_flush, "flush", "v", "f", "\n"
	    " void flush (file f)\n"
	    "\n"
	    " Force any pending output to be delivered to the OS.\n" },
        { do_File_getb, "getb", "i", "f", "\n"
	    " int getb (file f)\n"
	    "\n"
	    " Return the next byte of data from 'f'.\n" },
	{ do_File_getc, "getc", "i", "f", "\n"
	    " int getc (file f)\n"
	    "\n"
	    " Return the next character from 'f'.\n" },
        { do_File_string_read, "string_read", "f", "s", "\n"
	    " file string_read (string s)\n"
	    "\n"
	    " Create a file which when read will return successive"
	    " characters from 's'.\n" },
        { do_File_string_string, "string_string", "s", "f", "\n"
	    " string string_string (file f)\n"
	    "\n"
	    " Returns the contents 'f', which must be a file that was\n"
	    " created with string_write.\n" },
	{ do_File_isatty, "isatty", "b", "f", "\n"
	    " bool isatty (file f)\n"
	    "\n"
	    " Return whether 'f' is associated with an interactive\n"
	    " terminal device\n" },
	{ do_File_unlink, "unlink", "v", "s", "\n"
	    " void unlink (string name)\n"
	    "\n"
	    " Delete the filename 'name'\n" },
	{ do_File_rmdir, "rmdir", "v", "s", "\n"
	    " void rmdir (string name)\n"
	    "\n"
	    " Delete the directory 'name'\n" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_File_open, "open", "f", "ss", "\n"
	    " file open (string name, string mode)\n"
	    "\n"
	    " Open file 'name' where 'mode' is one of:\n"
	    "   \"r\":  read only\n"
	    "   \"r+\": read-write\n"
	    "   \"w\":  write only (created or truncated)\n"
	    "   \"w+\": read-write (created or truncated)\n"
	    "   \"a\":  write-only for appending (created if needed)\n"
	    "   \"a+\": read-write for appending (created if needed)\n"
	    " Raises open_error if the file cannot be opened.\n" },
        { do_File_putb, "putb", "i", "if", "\n"
	    " int putb (int b, file f)\n"
	    "\n"
	    " Write the byte 'b' to 'f'.\n" },
	{ do_File_putc, "putc", "i", "if", "\n"
	    " int putc (int c, file f)\n"
	    "\n"
	    " Write the character 'c' to 'f'.\n" },
        { do_File_setbuf, "setbuffer", "i", "fi", "\n"
	    " int setbuffer (file f, int mode)\n"
	    "\n"
	    " Change buffering of 'f' to 'mode', which is one of:\n"
	    "   0:    normal block buffering.\n"
	    "   1:    line buffering.\n"
	    "   2:    unbuffered.\n"
	    " Returns 'mode'.\n" },
        { do_File_ungetb, "ungetb", "i", "if", "\n"
	    " int ungetb (int b, file f)\n"
	    "\n"
	    " Pushes the byte 'b' back on file 'f'.\n" },
	{ do_File_ungetc, "ungetc", "i", "if", "\n"
	    " int ungetc (int c, file f)\n"
	    "\n"
	    " Pushes the character 'c' back on file 'f'.\n" },
	{ do_File_rename, "rename", "v", "ss", "\n"
	    " void rename (string oldname, string newname)\n"
	    "\n"
	    " Renames a file\n" },
	{ do_File_mkdir, "mkdir", "v", "si", "\n"
	    " void mkdir (string name, int mode)\n"
	    "\n"
	    " Create the new directory 'name'\n" },
        { 0 }
    };

    static const struct fbuiltin_3 funcs_3[] = {
        { do_File_filter, "filter", "i", "sA*sA*f", "\n"
	    " int filter (string program, string[*] argv, file[3] f)\n"
	    "\n"
	    " Fork and execute 'program' using 'argv' for arguments and\n"
	    " f as stdin, stdout and stderr (respectively).\n" },
	{ do_File_reopen, "reopen", "f", "ssf", "\n"
	    " file reopen (string name, string mode, file f)\n"
	    "\n"
	    " Change which file 'f' is associated with to 'name',\n"
	    " opening it with 'mode' as in the open function.\n" },
        { 0 }
    };

    static const struct fbuiltin_7 funcs_7[] = {
        { do_File_print, "print", "v", "fpsiiis", "\n"
	    " void print (file f, poly v, string format, int base,\n"
	    "             int width, int precision, string fill)\n"
	    "\n"
	    " Print 'v' to 'f'.\n"
	    " 'format' is one of:\n"
	    "    \"v\":   Reparsable representation.\n"
	    "    \"g\":   Human readable representation.\n"
	    "    \"d\":   Integer portion of a number.\n"
	    "    \"f\":   iii.fff format floating point.\n"
	    "    \"e\":   i.fff'e'ee format floating point.\n"
	    "    \"c\":   Integer printed as a character.\n"
	    "    \"s\":   String printed without quotes.\n"
	    " Any number will be represented using 'base' numerals.\n"
	    " The output will be at most 'width' characters unless it\n"
	    " won't fit in that size.\n"
	    " Any decimal part will be limited to 'precision' characters.\n"
	    " If 'precision' is -1, precision will be set to the default (10)\n"
	    " If 'precision' is -2, precision will set to whatever is needed \n"
	    " to precisely represent the number.\n"
	    " Any extra characters will be filled with 'fill'.\n" },
        { 0 }
    };

    static const struct ebuiltin excepts[] = {
	{"open_error",		exception_open_error,		"sEs", "\n"
	    " open_error (string message, error_type error, string name)\n"
	    "\n"
	    " Raised when an open attempt fails.\n"
	    " 'message' is a printable error string.\n"
	    " 'error' is a symbolic error code.\n"
	    " 'name' is the filename which failed.\n" },
	{"io_error",		exception_io_error,		"sEf", "\n"
	    " io_error (string message, error_type error, file f)\n"
	    "\n"
	    " Raised when an i/o error occurs.\n" },
	{"name_error",		exception_name_error,		"sEs", "\n"
	    " name_error (string message, error_type error, string name)\n"
	    "\n"
	    " Raised when an operation involving a filename fails.\n"
	    " 'message' is a printable error string.\n"
	    " 'error' is a symbolic error code.\n"
	    " 'name' is the filename which failed.\n" },
	{"io_eof",		exception_io_eof,		"f", "\n"
	    " io_eof (file f)\n"
	    "\n"
	    " Raised when reading at end-of-file.\n"
	    " 'file' is the file at eof.\n" },
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
	BuiltinAddException (&FileNamespace, e->exception, e->name, e->args, e->doc);

    s = typeFileError->name.name;
    NamespaceAddName (FileNamespace, s, publish_public);
    
    s = NewSymbolType (AtomId("errorType"), typeFileError);
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
    if (ibase < 0 || ibase == 1)
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("Illegal base"),
				NewInt (0), base);
	return Void;
    }
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
	Print (file, value, 
	       StringGet (StringChars (&format->string),
			  format->string.length, 0),
	       ibase, iwidth,
	       iprec,
	       StringGet (StringChars (&fill->string),
			  fill->string.length, 0));
	if (file->file.flags & FileOutputError)
	{
	    RaiseStandardException (exception_io_error, 3,
				    FileGetErrorMessage (file->file.output_errno), 
				    FileGetError (file->file.output_errno),
				    file);
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

    n = StrzPart (name, "invalid file name");
    if (!n)
	RETURN (Void);
    m = StrzPart (mode, "invalid file mode");
    if (!m)
	RETURN (Void);
    if (aborting)
	RETURN (Void);
    ret = FileFopen (n, m, &err);
    if (!ret)
    {
	RaiseStandardException (exception_open_error, 3,
				FileGetErrorMessage (err),
				FileGetError (err),
				name);
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
	RaiseStandardException (exception_io_error, 3,
				FileGetErrorMessage (f->file.output_errno), 
				FileGetError (f->file.output_errno), f);
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
	RaiseStandardException (exception_io_error, 3,
				FileGetErrorMessage (f->file.output_errno), 
				FileGetError (f->file.output_errno), f);
	break;
    default:
	if (FileClose (f) == FileError)
	{
	    RaiseStandardException (exception_io_error, 3,
				    FileGetErrorMessage (f->file.output_errno), 
				    FileGetError (f->file.output_errno), f);
	}
	else
	    complete = True;
    }
    return Void;
}

Value
do_File_filter (Value path, Value argv, Value filev)
{
    ENTER ();
    char    *p = StrzPart (path, "invalid program path");
    char    **args;
    int	    argc;
    Value   arg;
    Value   ret;
    int	    err;

    if (!p)
	RETURN (Void);
    
    /* set up arguments */
    args = AllocateTemp ((ArrayLimits(&argv->array)[0] + 1) * sizeof (char *));
    for (argc = 0; argc < ArrayLimits(&argv->array)[0]; argc++)
    {
	arg = ArrayValue (&argv->array, argc);
	args[argc] = StrzPart (arg, "invalid argument");
	if (!args[argc])
	    RETURN (Void);
    }
    args[argc] = 0;

    /* run the filter */
    if (aborting)
	RETURN(Void);
    ret = FileFilter (p, args, filev, &err);
    if (!ret)
    {
	RaiseStandardException (exception_open_error, 3,
				FileGetErrorMessage (err),
				FileGetError (err), path);
	ret = Void;
    }
    complete = True;
    RETURN (ret);
}

Value do_File_mkpipe (void) {
    ENTER ();
    int err;
    Value ret;
    
    if (aborting)
	RETURN (Void);
    ret = FileMakePipe (&err);
    if (!ret)
    {
	RaiseStandardException (exception_open_error, 3,
				FileGetErrorMessage (err),
				FileGetError (err), Void);
	RETURN (Void);
    }
    RETURN (ret);
}

Value
do_File_reopen (Value name, Value mode, Value file)
{
    ENTER ();
    char	*n, *m;
    Value	ret;
    int		err;

    n = StrzPart (name, "invalid file name");
    if (!n)
	RETURN (Void);
    m = StrzPart (mode, "invalid file mode");
    if (!m)
	RETURN (Void);
    if (aborting)
	RETURN (Void);
    ret = FileReopen (n, m, file, &err);
    if (!ret)
    {
	RaiseStandardException (exception_open_error, 3,
				FileGetErrorMessage (err),
				FileGetError (err), name);
	RETURN (Void);
    }
    complete = True;
    RETURN (ret);
}

Value
do_File_string_read (Value s)
{
    ENTER ();

    RETURN (FileStringRead (StringChars (&s->string), s->string.length));
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
	    RaiseStandardException (exception_io_error, 3,
				    FileGetErrorMessage (f->file.input_errno),
				    FileGetError (f->file.input_errno), f);
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
	    RaiseStandardException (exception_io_error, 3,
				    FileGetErrorMessage (f->file.input_errno),
				    FileGetError (f->file.input_errno), f);
	    RETURN (Void);
	case FileEOF:
	    RaiseStandardException (exception_io_eof, 1, f);
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
    {
	Value   b = do_File_getb(f);

	if (b == Void)
	    RETURN(Void);
	
	do_File_ungetb (b, f);
	if (f->file.flags & FileEnd)
	    RETURN (TrueVal);
	RETURN (FalseVal);
    }
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
		RaiseStandardException (exception_io_error, 3,
					FileGetErrorMessage (f->file.output_errno),
					FileGetError (f->file.output_errno),
					f);
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
		RaiseStandardException (exception_io_error, 3,
					FileGetErrorMessage (f->file.output_errno),
					FileGetError (f->file.output_errno),
					f);
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
    
    if (!aborting)
    {
	complete = True;
	FileUnput (f, IntPart (v, "ungetb: non integer"));
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

Value
do_File_unlink (Value name)
{
    ENTER ();
    char *n;
    int ret;

    n = StrzPart (name, "invalid file name");
    if (!n)
	RETURN (Void);
    ret = unlink (n);
    if (ret < 0) {
	int err = errno;
	RaiseStandardException (exception_name_error, 3,
				FileGetErrorMessage (err),
				FileGetError (err), name);
	RETURN (Void);
    }
    RETURN (Void);
}

Value
do_File_rename (Value old, Value new)
{
    ENTER ();
    char *o, *n;
    int ret;

    o = StrzPart (old, "invalid file name");
    if (!o)
	RETURN (Void);
    n = StrzPart (new, "invalid file name");
    if (!n)
	RETURN (Void);
    ret = rename (o, n);
    if (ret < 0) {
	int err = errno;
	RaiseStandardException (exception_name_error, 3,
				FileGetErrorMessage (err),
				FileGetError (err), new);
	RETURN (Void);
    }
    RETURN (Void);
}

Value
do_File_mkdir (Value name, Value mode)
{
    ENTER ();
    char *n;
    int m;
    int ret;

    n = StrzPart (name, "invalid file name");
    if (!n)
	RETURN (Void);
    m = IntPart (mode, "invalid file mode");
    if (aborting)
	RETURN (Void);
    ret = mkdir (n, m);
    if (ret < 0) {
	int err = errno;
	RaiseStandardException (exception_name_error, 3,
				FileGetErrorMessage (err),
				FileGetError (err), name);
	RETURN (Void);
    }
    RETURN (Void);
}

Value
do_File_rmdir (Value name)
{
    ENTER ();
    char *n;
    int ret;

    n = StrzPart (name, "invalid file name");
    if (!n)
	RETURN (Void);
    ret = rmdir (n);
    if (ret < 0) {
	int err = errno;
	RaiseStandardException (exception_name_error, 3,
				FileGetErrorMessage (err),
				FileGetError (err), name);
	RETURN (Void);
    }
    RETURN (Void);
}
