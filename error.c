/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	"gram.h"

Bool	abortError;

static void
fprintType (Value f, Type tag)
{
    switch (tag) {
    case type_undef:
	FilePuts (f, "poly");
	break;
    case type_int:
    case type_integer:
	FilePuts (f, "integer");
	break;
    case type_rational:
	FilePuts (f, "rational");
	break;
    case type_float:
	FilePuts (f, "real");
	break;
    case type_string:
	FilePuts (f, "string");
	break;
    case type_file:
	FilePuts (f, "file");
	break;
    case type_thread:
	FilePuts (f, "thread");
	break;
    case type_mutex:
	FilePuts (f, "mutex");
	break;
    case type_semaphore:
	FilePuts (f, "semaphore");
	break;
    case type_continuation:
	FilePuts (f, "continuation");
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

static void
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
    case class_typedef:
	FilePuts (f, "typedef");
	break;
    case class_namespace:
	FilePuts (f, "namespace");
	break;
    case class_exception:
	FilePuts (f, "exception");
	break;
    }
}

static void
fprintPublish (Value f, Publish publish)
{
    switch (publish) {
    case publish_private:
	FilePuts (f, "private");
	break;
    case publish_public:
	FilePuts (f, "public");
	break;
    case publish_extend:
	FilePuts (f, "extend");
	break;
    }
}

static void
fprintArgTypes (Value f, ArgType *at)
{
    while (at)
    {
	if (at->type)
	{
	    fprintTypes (f, at->type);
	    if (at->name)
		FilePuts (f, " ");
	}
	if (at->name)
	    FilePuts (f, AtomName (at->name));
	if (at->varargs)
	    FilePuts (f, " ...");
	at = at->next;
	if (at)
	    FilePuts (f, ", ");
    }
}

static void
fprintDimensions (Value f, ExprPtr dims)
{
    while (dims)
    {
	if (dims->tree.left)
	    printExpr (f, dims->tree.left, -1, 0, False);
	else
	    FilePuts (f, "*");
	if (dims->tree.right)
	    FilePuts (f, ", ");
	dims = dims->tree.right;
    }
}

void
fprintTypes (Value f, Types *t)
{
    int		    i;
    StructType	    *st;
    StructElement   *se;
    
    if (!t)
    {
	fprintType (f, type_undef);
    } 
    else
    {
	switch (t->base.tag) {
	case types_prim:
	    fprintType (f, t->prim.prim);
	    break;
	case types_name:
	    FilePuts (f, AtomName (t->name.name));
	    break;
	case types_ref:
	    FilePuts (f, "*");
	    fprintTypes (f, t->ref.ref);
	    break;
	case types_func:
	    fprintTypes (f, t->func.ret);
	    FilePuts (f, "(");
	    fprintArgTypes (f, t->func.args);
	    FilePuts (f, ")");
	    break;
	case types_array:
	    fprintTypes (f, t->array.type);
	    FilePuts (f, "[");
	    fprintDimensions (f, t->array.dimensions);
	    FilePuts (f, "]");
	    break;
	case types_struct:
	case types_union:
	    if (t->base.tag == types_union)
		FilePuts (f, "union { ");
	    else
		FilePuts (f, "struct { ");
	    st = t->structs.structs;
	    se = StructTypeElements (st);
	    for (i = 0; i < st->nelements; i++)
	    {
		fprintTypes (f, se[i].type);
		FilePuts (f, " ");
		FilePuts (f, AtomName (se[i].name));
		FilePuts (f, "; ");
	    }
	    FilePuts (f, "}");
	    break;
	}
    }
}

static void
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

static void
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
    Types	*type;
    Value	v;
    Class	storage;
    Publish	publish;
    Bool	newline = True;

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
	    case 't':
		type = va_arg (args, Types *);
		fprintTypes (FileStderr, type);
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
	    case 'z':
		newline = False;
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
    if (newline)
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
