/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	builtin.c
 *
 *	initialize builtin functions
 */

#include	<config.h>

#include	<math.h>
#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"nickle.h"

#ifndef PI
# define PI	3.14159265358979323846
#endif

ScopePtr    PrimitiveScope;
ScopePtr    DebugScope;
ScopePtr    ThreadScope;
ScopePtr    MutexScope;
ScopePtr    SemaphoreScope;
ScopePtr    MathScope;
ScopePtr    StringScope;

struct fbuiltin_v {
    Value	(*func) (int, Value *);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_0 {
    Value	(*func) (void);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_1 {
    Value	(*func) (Value);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_2 {
    Value	(*func) (Value, Value);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_3 {
    Value	(*func) (Value, Value, Value);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_4 {
    Value	(*func) (Value, Value, Value, Value);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_5 {
    Value	(*func) (Value, Value, Value, Value, Value);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_6 {
    Value	(*func) (Value, Value, Value, Value, Value, Value);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_7 {
    Value	(*func) (Value, Value, Value, Value, Value, Value, Value);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_vj {
    Value	(*func) (InstPtr *, int, Value *);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_0j {
    Value	(*func) (InstPtr *);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_1j {
    Value	(*func) (InstPtr *, Value);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct fbuiltin_2j {
    Value	(*func) (InstPtr *, Value, Value);
    char	*name;
    Type	ret;
    char	*args;
    ScopePtr	*scope;
};

struct dbuiltin {
    double	bd_value;
    char	*bd_name;
    ScopePtr	*bd_scope;
};

struct sbuiltin {
    char	*bs_value;
    char	*bs_name;
    ScopePtr	*bs_scope;
};

struct ibuiltin {
    char	*is_name;
    int		is_file;
    ScopePtr	*is_scope;
};

struct nbuiltin {
    char	*bn_name;
    ScopePtr	*bn_value;
    ScopePtr	*bn_scope;
};
struct fbuiltin_v funcs_v[] = {
    { doprintf,		"printf",	type_int,   "s." },
    { doscanf,		"scanf",	type_int,   "s." },
    { dofprintf,	"fprintf",	type_int,   "fs." },
    { Kill,		"kill",		type_int,   ".", &ThreadScope },
    { Trace,		"trace",	type_int,   ".", &ThreadScope },
    { Trace,		"trace",	type_int,   ".", &DebugScope },
    { doHistoryShow,    "HistoryShow",	type_int,   "s." },
    { 0,		0 },
};
struct fbuiltin_0 funcs_0[] = {
    { Cont,		"cont",		type_int,   "", &ThreadScope },
    { CurrentThread,	"current",	type_int,   "", &ThreadScope },
    { NewMutex,		"new",		type_mutex, "", &MutexScope },
    { NewSemaphore,	"new",		type_semaphore,	"", &SemaphoreScope },
    { ThreadsList,	"list",		type_int,   "", &ThreadScope },
    { dogetchar,	"getchar",	type_int,   "" },
    { dotime,		"time",		type_int,   "" },
    { DebugUp,		"up",		type_int,   "", &DebugScope, },
    { DebugDown,	"down",		type_int,   "", &DebugScope },
    { DebugDone,	"done",		type_int,   "", &DebugScope },
    { 0,		0 },
};
struct fbuiltin_1 funcs_1[] = {
    { Atof,		"atof",		type_double,	"s" },
    { Atoi,		"atoi",		type_integer,	"s" },
    { GetPriority,	"getPriority",	type_int,	"t", &ThreadScope },
    { MutexAcquire,	"acquire",	type_int,	"m", &MutexScope },
    { MutexRelease,	"release",	type_int,	"m", &MutexScope },
    { MutexTryAcquire,	"tryAcquire",	type_int,	"m", &MutexScope },
    { SemaphoreSignal,	"signal",	type_int,	"S", &SemaphoreScope },
    { SemaphoreTest,	"test",		type_int,	"S", &SemaphoreScope },
    { SemaphoreWait,	"wait",		type_int,	"S", &SemaphoreScope },
    { Sleep,		"sleep",	type_int,	"n" },
    { ThreadFromId,	"threadFromId", type_thread,	"n", &ThreadScope },
    { ThreadJoin,	"join",		type_undef,	"t", &ThreadScope },
    { acosD,		"acos",		type_double,	"n", &MathScope },
    { asinD,		"asin",		type_double,	"n", &MathScope },
    { atanD,		"atan",		type_double,	"n", &MathScope },
    { ceilD,		"ceil", 	type_double,	"n" },
    { cosD,		"cos",		type_double,	"n", &MathScope },
    { coshD,		"cosh",		type_double,	"n", &MathScope },
    { dim,		"dim",		type_int,	"a" },
    { dims,		"dims",		type_array,	"a" },
    { doHistoryInsert,	"HistoryInsert",type_undef,	"p" },
    { dofclose,		"fclose",	type_int,	"f" },
    { dofflush,		"fflush",	type_int,	"f" },
    { dogetc,		"getc",		type_int,	"f" },
    { doputchar,	"putchar",	type_int,	"n" },
    { expD,		"exp",		type_double,	"n", &MathScope },
    { fabsD,		"abs",		type_double,	"n" },
    { floorD,		"floor",	type_integer,	"n" },
    { j0D,		"j0",		type_double,	"n", &MathScope },
    { j1D,		"j1",		type_double,	"n", &MathScope },
    { lengthS,          "length",	type_int,	"s", &StringScope },
    { log10D,		"log10",	type_double,	"n", &MathScope },
    { logD,		"log",		type_double,	"n", &MathScope },
    { sinD,		"sin",		type_double,	"n", &MathScope },
    { sinhD,		"sinh",		type_double,	"n", &MathScope },
    { sqrtD,		"sqrt",		type_double,	"n" },
    { tanD,		"tan",		type_double,	"n", &MathScope },
    { tanhD,		"tanh",		type_double,	"n", &MathScope },
    { y0D,		"y0",		type_double,	"n", &MathScope },
    { y1D,		"y1",		type_double,	"n", &MathScope },
    { _random,		"random",	type_int,	"n", &PrimitiveScope },
    { _srandom,		"srandom",	type_int,	"n", &PrimitiveScope },
    { 0,		0 },
};
struct fbuiltin_2 funcs_2[] = {
    { SetPriority,	"setPriority",	type_int,	"tn", &ThreadScope },
    { Strtol,		"strtol",	type_integer,	"sn" },
    { atan2D,		"atan2",	type_double,	"nn", &MathScope },
    { dofopen,		"fopen",	type_file,	"ss" },
    { dogcd,		"gcd",		type_integer,	"nn" },
    { doputc,		"putc",		type_int,	"nf" },
    { dosetbuf,		"setbuffer",	type_int,	"fn" },
    { hypotD,		"hypot",	type_double,	"nn", &MathScope },
    { indexS,           "index",	type_int,	"ss", &StringScope },
    { jnD,		"jn",		type_double,	"nn", &MathScope },
    { ynD,		"yn",		type_double,	"nn", &MathScope },
    { 0,		0 },
};
struct fbuiltin_3 funcs_3[] = {
    { substrS,          "substr",	type_string,	"snn", &StringScope },
};
struct fbuiltin_7 funcs_7[] = {
    { doprint,		"Print",	type_int,	"fpnnnns" },
    { 0,		0 },
};

struct fbuiltin_2j funcs_2j[] = {
    { SetJump,		"setjump",	type_undef,	"*cp" },
    { LongJump,		"longjump",	type_undef,	"cp" },
    { 0,		0 },
};

struct dbuiltin dvars[] = {
    { 3.1415926535897932384626433,  "pi" },
    { 2.7182818284590452353602874,  "e" },
    { 0.0,			    0 },
};

struct sbuiltin svars[] = {
    { "> ",	"prompt" },
    { "+ ",	"prompt2" },
    { "- ",	"prompt3" },
    { "%g",	"format" },
    { VERSION,	"version" },
#ifdef BUILD
    { BUILD,	"build" },
#else
    { "?",	"build" },
#endif
    { 0,    0 },
};

struct ibuiltin ivars[] = {
    { "stdin",	0 },
    { "stdout",	1 },
    { "stderr",	2 },
    { 0,	0 },
};

struct nbuiltin nvars[] = {
    { "primitive",  &PrimitiveScope },
    { "debugger",   &DebugScope },
    { "thread",	    &ThreadScope },
    { "math",	    &MathScope },
    { "mutex",	    &MutexScope },
    { "semaphore",  &SemaphoreScope },
    { "strings",    &StringScope },
    { 0,	    0 },
};

SymbolPtr
BuiltinSymbol (ScopePtr *scopep,
	       char	*name,
	       Types	*type)
{
    ENTER ();
    ScopePtr	scope;

    if (scopep)
	scope = *scopep;
    else
	scope = GlobalScope;
    RETURN (ScopeAddSymbol (scope,
			    NewSymbolGlobal (AtomId (name), type,
					     publish_public)));
}

SymbolPtr
BuiltinScope (ScopePtr	*scopep,
	      char	*name)
{
    ENTER ();
    ScopePtr	scope;

    if (scopep)
	scope = *scopep;
    else
	scope = GlobalScope;
    RETURN (ScopeAddSymbol (scope,
			    NewSymbolScope (AtomId (name), 
					    NewScope (0),
					    publish_public)));
}

ArgType *
BuiltinArgTypes (char *format, int *argcp)
{
    ENTER ();
    ArgType	*args, *a, **last;
    int		argc;
    Types	*t;
    Bool	ref;
    
    last = &args;
    argc = 0;
    while (*format)
    {
	if (*format == '.')
	{
	    argc = -1;
	    break;
	}
	ref = False;
	if (*format == '*')
	{
	    ref = True;
	    format++;
	}
	switch (*format++) {
	case 'p': t = NewTypesPrim (type_undef); break;
	case 'n': t = NewTypesPrim (type_double); break;
	case 's': t = NewTypesPrim (type_string); break;
	case 'f': t = NewTypesPrim (type_file); break;
	case 't': t = NewTypesPrim (type_thread); break;
	case 'm': t = NewTypesPrim (type_mutex); break;
	case 'S': t = NewTypesPrim (type_semaphore); break;
	case 'c': t = NewTypesPrim (type_continuation); break;
	case 'r': t = NewTypesPrim (type_ref); break;
	}
	if (ref)
	    t = NewTypesRef (t);
	argc++;
        a = NewArgType (t, 0, 0);
	*last = a;
	last = &a->next;
    }
    *argcp = argc;
    RETURN(args);
}

void
BuiltinAddFunction (ScopePtr *scopep, char *name, Type ret,
		    char *format, BuiltinFunc f)
{
    ENTER ();
    Value	func;
    SymbolPtr	sym;
    int		argc;
    ArgType	*args;

    args = BuiltinArgTypes (format, &argc);
    sym = BuiltinSymbol (scopep, name, NewTypesFunc (NewTypesPrim (ret), True, args));
    func =  NewFunc (NewBuiltinCode (NewTypesPrim (ret), args, argc, f, False), 0);
    BoxValue (sym->global.value, 0) = func;
    EXIT ();
}

void
BuiltinAddJumpingFunction (ScopePtr *scopep, char *name, Type ret,
			   char *format, BuiltinFunc f)
{
    ENTER ();
    Value	func;
    SymbolPtr	sym;
    ArgType	*args;
    int		argc;
    
    args = BuiltinArgTypes (format, &argc);
    sym = BuiltinSymbol (scopep, name, NewTypesFunc (NewTypesPrim(ret), True, args));
    func =  NewFunc (NewBuiltinCode (NewTypesPrim (ret), args, argc, f, True), 0);
    BoxValue (sym->global.value, 0) = func;
    EXIT ();
}

void
BuiltinInit (void)
{
    ENTER ();
    struct fbuiltin_v	*f_v;
    struct fbuiltin_0	*f_0;
    struct fbuiltin_1	*f_1;
    struct fbuiltin_2	*f_2;
    struct fbuiltin_3	*f_3;
    struct fbuiltin_7	*f_7;
    struct fbuiltin_2j	*f_2j;
    struct dbuiltin	*d;
    struct sbuiltin	*s;
    struct ibuiltin	*i;
    struct nbuiltin	*n;
    BuiltinFunc		f;
    SymbolPtr		sym;

    for (n = nvars; n->bn_name; n++) {
	sym = BuiltinScope (n->bn_scope, n->bn_name);
	*n->bn_value = sym->scope.scope;
    }
    for (f_v = funcs_v; f_v->name; f_v++) {
	f.builtinN = f_v->func;
	
	BuiltinAddFunction (f_v->scope, f_v->name, f_v->ret, f_v->args, f);
    }
    for (f_0 = funcs_0; f_0->name; f_0++) {
	f.builtin0 = f_0->func;
	BuiltinAddFunction (f_0->scope, f_0->name, f_0->ret, f_0->args, f);
    }
    for (f_1 = funcs_1; f_1->name; f_1++) {
	f.builtin1 = f_1->func;
	BuiltinAddFunction (f_1->scope, f_1->name, f_1->ret, f_1->args, f);
    }
    for (f_2 = funcs_2; f_2->name; f_2++) {
	f.builtin2 = f_2->func;
	BuiltinAddFunction (f_2->scope, f_2->name, f_2->ret, f_2->args, f);
    }
    for (f_3 = funcs_3; f_3->name; f_3++) {
	f.builtin3 = f_3->func;
	BuiltinAddFunction (f_3->scope, f_3->name, f_3->ret, f_3->args, f);
    }
    for (f_7 = funcs_7; f_7->name; f_7++) {
	f.builtin7 = f_7->func;
	BuiltinAddFunction (f_7->scope, f_7->name, f_7->ret, f_7->args, f);
    }
    for (f_2j = funcs_2j; f_2j->name; f_2j++) {
	f.builtin2J = f_2j->func;
	BuiltinAddJumpingFunction (f_2j->scope, f_2j->name, f_2j->ret, f_2j->args, f);
    }
    
    for (d = dvars; d->bd_name; d++) {
	sym = ScopeAddSymbol (GlobalScope, 
			      NewSymbolGlobal (AtomId (d->bd_name), 
					       NewTypesPrim (type_double), 
					       publish_private));
	sym->global.value->constant = True;
	BoxValue (sym->global.value, 0) = NewDouble (d->bd_value);
    }
    for (s = svars; s->bs_name; s++) {
	sym = BuiltinSymbol (s->bs_scope, s->bs_name, NewTypesPrim (type_string));
	BoxValue (sym->global.value, 0) = NewStrString (s->bs_value);
    }
    for (i = ivars; i->is_name; i++) {
	Value   f;
	
	switch (i->is_file) { 
	case 0: f = FileStdin; break; 
        case 1: f = FileStdout; break;
	default: f = FileStderr;  break;
	}
	sym = BuiltinSymbol (i->is_scope, i->is_name, NewTypesPrim (type_file));
	BoxValue (sym->global.value, 0) = f;
    }
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
		    RaiseError ("too few arguments to printf");
		    return -1;
		}
		width = IntPart (p[pn], "Illegal format width");
		if (exception)
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
		while (isdigit (c = *++fm))
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
			    RaiseError ("too few arguments to printf");
			    return -1;
			}
			prec = IntPart (p[pn], "Illegal format precision");
			if (exception)
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
			while (isdigit (c = *++fm))
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
		    RaiseError ("too few arguments to printf");
		    return -1;
		}
		break;
	    }
	}
	else
	    FileOutput (f, c);
    }
    if (pn != n)
	RaiseError ("too many arguments to printf");
    return -1;
}

void
print (Value f, Value v)
{
    dofformat (f, "%v", 1, &v);
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
	else if (!abortError)	/* only complete if no errors */
	    complete = True;
    }
}

Value 
doprintf (int n, Value *p)
{
    char	*fmt;
    
    if (n == 0 || p[0]->value.tag != type_string) {
	RaiseError ("first parameter to printf should be string");
	return Zero;
    }
    fmt = StringChars (&p[0]->string);
    p++;
    n--;
    callformat (FileStdout, fmt, n, p);
    return Zero;
}

Value 
dofprintf (int n, Value *p)
{
    Value	f;
    char	*fmt;
    
    if (n == 0 || p[0]->value.tag != type_file) {
	RaiseError ("first parameter to fprintf should be file");
	return Zero;
    }
    f = p[0];
    p++;
    n--;
    if (n == 0 || p[0]->value.tag != type_string) {
	RaiseError ("second parameter to fprintf should be string");
	return Zero;
    }
    fmt = StringChars(&p[0]->string);
    p++;
    n--;
    callformat (f, fmt, n, p);
    return Zero;
}

Value
doprint (Value file, Value value, Value format, 
	 Value base, Value width, Value prec, Value fill)
{
    int	    ibase, iwidth, iprec;
    
    if (file->value.tag != type_file)
    {
	RaiseError ("first parameter to print should be file");
	return Zero;
    }
    if (format->value.tag != type_string)
    {
	RaiseError ("third parameter to print should be format string");
	return Zero;
    }
    ibase = IntPart (base, "Illegal base");
    if (exception)
	return Zero;
    iwidth = IntPart (width, "Illegal width");
    if (exception)
	return Zero;
    iprec = IntPart (prec, "Illegal precision");
    if (exception)
	return Zero;
    if (fill->value.tag != type_string)
    {
	RaiseError ("seventh parameter to print should be fill string");
	return Zero;
    }
    if (file->file.flags & FileOutputBlocked)
	ThreadSleep (running, file, PriorityIo);
    else
	Print (file, value, *StringChars(format), ibase, iwidth,
	       iprec, *StringChars(fill));
    return Zero;
}

Value 
dofopen (Value name, Value mode)
{
    ENTER ();
    char	*n, *m;
    Value	ret;

    if (name->value.tag != type_string) {
	RaiseError ("first parameter to fopen should be file name");
	RETURN (Zero);
    }
    n = StringChars (&name->string);
    if (mode->value.tag != type_string) {
	RaiseError ("second parameter to fopen should be mode");
	RETURN (Zero);
    }
    m = StringChars (&mode->string);
    if (exception)
	RETURN (Zero);
    complete = True;
    ret = FileFopen (n, m);
    if (!ret)
	RETURN (Zero);
    RETURN (ret);
}

Value 
dofflush (Value f)
{
    if (f->value.tag != type_file) {
	RaiseError ("parameter to fflush should be file");
	return Zero;
    }
    if (FileFlush (f) == FileBlocked)
	ThreadSleep (running, f, PriorityIo);
    return One;
}

Value 
dofclose (Value f)
{
    if (f->value.tag != type_file) {
	RaiseError ("parameter to fclose should be file");
	return Zero;
    }
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
dogetc (Value f)
{
    ENTER ();
    int	    c;
    
    if (f->value.tag != type_file) {
	RaiseError ("parameter to getc should be file");
	RETURN (Zero);
    }
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
dogetchar ()
{
    ENTER ();
    RETURN (dogetc (FileStdin));
}

Value 
doputc (Value v, Value f)
{
    ENTER ();
    
    if (f->value.tag != type_file) {
	RaiseError ("parameter to putc should be file");
	RETURN (Zero);
    }
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
doputchar (Value v)
{
    ENTER ();
    RETURN (doputc (v, FileStdout));
}

Value
dosetbuf (Value f, Value v)
{
    ENTER ();
    int	i;

    if (f->value.tag != type_file) {
	RaiseError ("parameter to setbuffer should be file");
	RETURN (Zero);
    }
    i = IntPart (v, "setbuffer non integer");
    if (!aborting)
	FileSetBuffer (f, i);
    RETURN (v);
}

Value 
dogcd (Value a, Value b)
{
    ENTER ();
    RETURN (Gcd (a, b));
}

Value
dotime (void)
{
    ENTER ();
    RETURN (NewInt (time (0)));
}

Value
doHistoryInsert (Value a)
{
    ENTER ();
    HistoryInsert (a);
    complete = True;
    RETURN (a);
}

Value
doHistoryShow (int n, Value *p)
{
    ENTER ();

    switch (n) {
    case 1:
	HistoryDisplay (p[0], (Value *) 0, (Value *) 0);
	break;
    case 2:
	HistoryDisplay (p[0], p + 1, (Value *) 0);
	break;
    case 3:
	HistoryDisplay (p[0], p + 1, p + 2);
	break;
    }
    RETURN (Zero);
}

Value 
/*ARGSUSED*/
doscanf (int n, Value *p)
{
    return Zero;
}

extern Value	atov (char *, int);
extern Value	aetov (char *);

Value
Strtol (Value str, Value base)
{
    ENTER ();
    char    *s;
    int	    ibase;
    int	    negative = 0;
    Value   ret = Zero;

    if (str->value.tag != type_string)
    {
	RaiseError ("strtol: first argument must be string %v", str);
    }
    else
    {
	s = StringChars (&str->string);
	while (isspace (*s)) s++;
	switch (*s) {
	case '-':
	    negative = 1;
	    s++;
	    break;
	case '+':
	    s++;
	    break;
	}
	ibase = IntPart (base, "strtol: invalid base");
	if (!exception)
	{
	    if (ibase == 0)
	    {
		if (!strncmp (s, "0x", 2) ||
		    !strncmp (s, "0X", 2)) ibase = 16;
		else if (!strncmp (s, "0t", 2) ||
			 !strncmp (s, "0T", 2)) ibase = 10;
		else if (!strncmp (s, "0b", 2) ||
			 !strncmp (s, "0B", 2)) ibase = 2;
		else if (!strncmp (s, "0o", 2) ||
			 !strncmp (s, "0O", 2) ||
			 *s == '0') ibase = 8;
		else ibase = 10;
	    }
	    switch (ibase) {
	    case 2:
		if (!strncmp (s, "0b", 2) ||
		    !strncmp (s, "0B", 2)) s += 2;
		break;
	    case 8:
		if (!strncmp (s, "0o", 2) ||
		    !strncmp (s, "0O", 2)) s += 2;
	    case 10:
		if (!strncmp (s, "0t", 2) ||
		    !strncmp (s, "0T", 2)) s += 2;
		break;
	    case 16:
		if (!strncmp (s, "0x", 2) ||
		    !strncmp (s, "0X", 2)) s += 2;
		break;
	    }
	    ret = atov (s, ibase);
	    if (!exception)
	    {
		if (negative)
		    ret = Negate (ret);
	    }
	}
    }
    RETURN (ret);
}

Value
Atoi (Value str)
{
    ENTER ();
    if (str->value.tag != type_string)
    {
	RaiseError ("atoi: argument must be string %v", str);
	RETURN (Zero);
    }
    RETURN (Strtol (str, Zero));
}

Value
Atof (Value str)
{
    ENTER ();
    if (str->value.tag != type_string)
    {
	RaiseError ("atof: argument must be string %v", str);
	RETURN (Zero);
    }
    RETURN (aetov (StringChars (&str->string)));
}

Value 
expD (Value a)
{
    ENTER ();
    RETURN (NewDouble (exp (DoublePart (a))));
}

Value 
logD (Value a)
{
    ENTER ();
    RETURN (NewDouble (log (DoublePart (a))));
}

Value 
log10D (Value a)
{
    ENTER ();
    RETURN (NewDouble (log10 (DoublePart (a))));
}

Value 
sqrtD (Value a)
{
    ENTER ();
    RETURN (NewDouble (sqrt (DoublePart (a))));
}

Value 
fabsD (Value a)
{
    ENTER ();
    RETURN (NewDouble (fabs (DoublePart (a))));
}

Value 
floorD (Value a)
{
    return Floor (a);
}

Value 
ceilD (Value a)
{
    return Ceil (a);
}

Value 
hypotD (Value a, Value b)
{
    ENTER ();
    RETURN (NewDouble (hypot (DoublePart (a), DoublePart (b))));
}

Value 
j0D (Value a)
{
    ENTER ();
    RETURN (NewDouble (j0 (DoublePart (a))));
}

Value 
j1D (Value a)
{
    ENTER ();
    RETURN (NewDouble (j1 (DoublePart (a))));
}

Value 
jnD (Value a, Value b)
{
    ENTER ();
    RETURN (NewDouble (jn (DoublePart (a), IntPart (b, "Non-integer to jn"))));
}

Value 
y0D (Value a)
{
    ENTER ();
    RETURN (NewDouble (y0 (DoublePart (a))));
}

Value 
y1D (Value a)
{
    ENTER ();
    RETURN (NewDouble (y1 (DoublePart (a))));
}

Value 
ynD (Value a, Value b)
{
    ENTER ();
    RETURN (NewDouble (yn (DoublePart (a), IntPart (b, "Non-integer to yn"))));
}

Value 
sinD (Value a)
{
    ENTER ();
    RETURN (NewDouble (sin (DoublePart (a))));
}

Value 
cosD (Value a)
{
    ENTER ();
    RETURN (NewDouble (cos (DoublePart (a))));
}

Value 
tanD (Value a)
{
    ENTER ();
    RETURN (NewDouble (tan (DoublePart (a))));
}

Value 
asinD (Value a)
{
    ENTER ();
    RETURN (NewDouble (asin (DoublePart (a))));
}

Value 
acosD (Value a)
{
    ENTER ();
    RETURN (NewDouble (acos (DoublePart (a))));
}

Value 
atanD (Value a)
{
    ENTER ();
    RETURN (NewDouble (atan (DoublePart (a))));
}

Value 
atan2D (Value a, Value b)
{
    ENTER ();
    RETURN (NewDouble (atan2 (DoublePart (a), DoublePart (b))));
}

Value 
sinhD (Value a)
{
    ENTER ();
    RETURN (NewDouble (sinh (DoublePart (a))));
}

Value 
coshD (Value a)
{
    ENTER ();
    RETURN (NewDouble (cosh (DoublePart (a))));
}

Value 
tanhD (Value a)
{
    ENTER ();
    RETURN (NewDouble (tanh (DoublePart (a))));
}

Value
_random (Value bits)
{
    ENTER();
    int n = IntPart (bits, "random: modulus non-integer");
    Value ret = Zero;

    if (n > 31)
	RaiseError ("random: modulus 2^%v exceeds 2^31", bits);
    else if (n <= 0)
	RaiseError ("random: bad modulus 2^%v", bits);
    else
	ret = NewInt (random () & ((1 << n) - 1));
    RETURN (ret);
}

Value
_srandom (Value seed)
{
    ENTER();
    int n = IntPart (seed, "srandom: non-integer seed");

    srandom ((unsigned int) n);
    RETURN (Zero);
}

Value
lengthS (Value av)
{
    ENTER();
    Value ret;
    if (av->value.tag != type_string)
    {
	RaiseError ("index: target must be string %v", av);
	RETURN (Zero);
    }
    ret = NewInt(strlen(StringChars(&av->string)));
    RETURN (ret);
}

Value
indexS (Value av, Value bv)
{
    ENTER();
    char *a, *b, *p;
    Value ret;
    if (av->value.tag != type_string)
    {
	RaiseError ("index: target must be string %v", av);
	RETURN (Zero);
    }
    a = StringChars(&av->string);
    if (bv->value.tag != type_string)
    {
	RaiseError ("index: pattern must be string %v", bv);
	RETURN (Zero);
    }
    b = StringChars(&bv->string);
    p = strstr(a, b);
    if (!p)
	RETURN (NewInt(-1));
    ret = NewInt(p - a);
    RETURN (ret);
}

Value
substrS (Value av, Value bv, Value cv)
{
    ENTER();
    char *a, *rchars;
    int b, c, al;
    Value ret;
    if (av->value.tag != type_string)
    {
	RaiseError ("substr: target must be string %v", av);
	RETURN (Zero);
    }
    a = StringChars(&av->string);
    al = strlen(a);
    b = IntPart(bv, "substr: index not integer");
    if (b < 0 || b >= al) {
	RaiseError ("substr: index out of bounds", av);
	RETURN (Zero);
    }
    c = IntPart(cv, "substr: count not integer");
    if (c < 0 || b + c > al) {
	RaiseError ("substr: count out of range", av);
	RETURN (Zero);
    }
    ret = NewString(c);
    rchars = StringChars(&ret->string);
    strncpy(rchars, a + b, c);
    rchars[c] = '\0';
    RETURN (ret);
}

Value
dim(Value av) {
    ENTER();
    Value ret;
    if (av->value.tag != type_array)
    {
	RaiseError ("dim: argument not array %v", av);
	RETURN (Zero);
    }
    if (av->array.ndim != 1)
    {
	RaiseError ("dim: argument must be one-dimensional array %v", av);
	RETURN (Zero);
    }
    ret = NewInt(av->array.dim[0]);
    RETURN (ret);
}

Value
dims(Value av) {
    ENTER();
    Value ret;
    int i;
    if (av->value.tag != type_array)
    {
	RaiseError ("dim: argument not array %v", av);
	RETURN (Zero);
    }
    ret = NewArray(True, type_int, 1, &av->array.ndim);
    for (i = 0; i < av->array.ndim; i++) {
      Value d = NewInt(av->array.dim[i]);
      BoxValue(ret->array.values, i) = d;
    }
    RETURN (ret);
}

