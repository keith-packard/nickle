/*
 * $Id$
 *
 * Copyright 1996 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

# include   "value.h"

Bool	abortError;

void
fprintType (Value f, Type tag)
{
    switch (tag) {
    case type_undef:
	FilePuts (f, "polymorph");
	break;
    case type_int:
	FilePuts (f, "int");
	break;
    case type_integer:
	FilePuts (f, "integer");
	break;
    case type_rational:
	FilePuts (f, "ratio");
	break;
    case type_double:
	FilePuts (f, "double");
	break;
    case type_string:
	FilePuts (f, "string");
	break;
    case type_array:
	FilePuts (f, "array");
	break;
    case type_ref:
	FilePuts (f, "ref");
	break;
    case type_struct:
	FilePuts (f, "struct");
	break;
    case type_func:
	FilePuts (f, "function");
	break;
    default:
	FilePrintf (f, "bad type %d", tag);
	break;
    }
}

void
fprintClass (Value f, Class storage)
{
    switch (storage) {
    case class_undef:
	FilePuts (f, "undefined");
	break;
    case class_global:
	FilePuts (f, "global");
	break;
    case class_arg:
	FilePuts (f, "argument");
	break;
    case class_auto:
	FilePuts (f, "auto");
	break;
    case class_static:
	FilePuts (f, "static");
	break;
    case class_struct:
	FilePuts (f, "struct");
	break;
    case class_scope:
	FilePuts (f, "scope");
	break;
    }
}

void
fprintPublish (Value f, Publish publish)
{
    switch (publish) {
    case publish_private:
	FilePuts (f, "private");
	break;
    case publish_public:
	FilePuts (f, "public");
	break;
    }
}

void
fprintBinOp (Value f, BinaryOp o)
{
    switch (o) {
    case PlusOp:
	FilePuts (f, "+");
	break;
    case MinusOp:
	FilePuts (f, "-");
	break;
    case TimesOp:
	FilePuts (f, "*");
	break;
    case DivideOp:
	FilePuts (f, "/");
	break;
    case DivOp:
	FilePuts (f, "//");
	break;
    case ModOp:
	FilePuts (f, "%");
	break;
    case LessOp:
	FilePuts (f, "<");
	break;
    case EqualOp:
	FilePuts (f, "==");
	break;
    case LandOp:
	FilePuts (f, "&");
	break;
    case LorOp:
	FilePuts (f, "|");
	break;
    default:
	break;
    }
}

void
fprintUnaryOp (Value f, UnaryOp o)
{
    switch (o) {
    case NegateOp:
	FilePuts (f, "-");
	break;
    case FloorOp:
	FilePuts (f, "floor");
	break;
    case CeilOp:
	FilePuts (f, "ceil");
	break;
    default:
	break;
    }
}

void
vPrintError (char *s, va_list args)
{
    Type	tag;
    Value	v;
    Class	storage;
    Publish	publish;

    for (;*s;) {
	switch (*s) {
	case '\0':
	    continue;
	case '%':
	    switch (*++s) {
	    case '\0':
		continue;
	    case 'd':
		FilePutInt (FileStderr, va_arg (args, int));
		break;
	    case 'v':
		v = va_arg (args, Value);
		if (!v)
		    FilePuts (FileStderr, "(null)");
		else
		    Print (FileStderr, v, 'v', 0, 0, DEFAULT_OUTPUT_PRECISION, ' ');
		break;
	    case 's':
		FilePuts (FileStderr, va_arg (args, char *));
		break;
	    case 'A':
		FilePuts (FileStderr, AtomName (va_arg (args, Atom)));
		break;
	    case 'T':
		tag = (Type) (va_arg (args, int));
		fprintType (FileStderr, tag);
		break;
	    case 'C':
		storage = (Class) (va_arg (args, int));
		fprintClass (FileStderr, storage);
		break;
	    case 'P':
		publish = (Publish) (va_arg (args, int));
		fprintPublish (FileStderr, publish);
		break;
	    case 'O':
		fprintBinOp (FileStderr, va_arg (args, BinaryOp));
		break;
	    case 'U':
		fprintUnaryOp (FileStderr, va_arg (args, UnaryOp));
		break;
	    case 'c':
		FileOutput (FileStderr, va_arg (args, int));
		break;
	    default:
		FileOutput (FileStderr, *s);
		break;
	    }
	    break;
	default:
	    FileOutput (FileStderr, *s);
	    break;
	}
	++s;
    }
    FileOutput (FileStderr, '\n');
}

void
PrintError (char *s, ...)
{
    va_list args;

    va_start (args, s);
    vPrintError (s, args);
    va_end (args);
}

void
RaiseError (char *s, ...)
{
    va_list	args;

    va_start (args, s);
    vPrintError (s, args);
    va_end (args);
    aborting = True;
    exception = True;
    abortError = True;
}
