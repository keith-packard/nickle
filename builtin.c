/* $Header$ */
/*
 * This program is Copyright (C) 1988 by Keith Packard.  NICK is provided to
 * you without charge, and with no warranty.  You may give away copies of
 * NICK, including source, provided that this notice is included in all the
 * files.
 */
/*
 *	builtin.c
 *
 *	initialize builtin functions
 */

# include	<math.h>
# include	<ctype.h>
# include	<strings.h>
# include	<time.h>
# include	"nick.h"

#ifndef PI
# define PI	3.14159265358979323846
#endif

ScopePtr    PrimitiveScope;
ScopePtr    DebugScope;

struct fbuiltin_v {
    Value	(*bf_func) (int, Value *);
    char	*bf_name;
    ScopePtr	*bf_scope;
};

struct fbuiltin_0 {
    Value	(*bf_func) (void);
    char	*bf_name;
    ScopePtr	*bf_scope;
};

struct fbuiltin_1 {
    Value	(*bf_func) (Value);
    char	*bf_name;
    ScopePtr	*bf_scope;
};

struct fbuiltin_2 {
    Value	(*bf_func) (Value, Value);
    char	*bf_name;
    ScopePtr	*bf_scope;
};

struct fbuiltin_3 {
    Value	(*bf_func) (Value, Value, Value);
    char	*bf_name;
    ScopePtr	*bf_scope;
};

struct fbuiltin_4 {
    Value	(*bf_func) (Value, Value, Value, Value);
    char	*bf_name;
    ScopePtr	*bf_scope;
};

struct fbuiltin_5 {
    Value	(*bf_func) (Value, Value, Value, Value, Value);
    char	*bf_name;
    ScopePtr	*bf_scope;
};

struct fbuiltin_6 {
    Value	(*bf_func) (Value, Value, Value, Value, Value, Value);
    char	*bf_name;
    ScopePtr	*bf_scope;
};

struct fbuiltin_7 {
    Value	(*bf_func) (Value, Value, Value, Value, Value, Value, Value);
    char	*bf_name;
    ScopePtr	*bf_scope;
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

Value	Atof (Value);
Value	Atoi (Value);
Value	Cont (void);
Value	CurrentThread (void);
Value	GetPriority (Value);
Value	Kill (int, Value *);
Value	SetPriority (Value,Value);
Value	Sleep(Value);
Value	Strtol (Value, Value);
Value	ThreadsList (void);
Value	Trace (int, Value *);
Value	acosD(Value);
Value	asinD(Value);
Value	atan2D(Value, Value);
Value	atanD(Value);
Value	ceilD(Value);
Value	cosD(Value);
Value	coshD(Value);
Value	doHistoryInsert(Value);
Value	doHistoryShow(int,Value*);
Value	dofclose(Value);
Value	dofflush(Value);
Value	dofopen(Value,Value);
Value	dofprintf(int, Value *);
Value	dogcd(Value,Value);
Value	dogetc(Value);
Value	dogetchar(void);
Value	dotime(void);
Value	doprint(Value,Value,Value,Value,Value,Value,Value);
Value	doprintf(int,Value *);
Value	doputc(Value,Value);
Value	doputchar(Value);
Value	doscanf(int,Value *);
Value	expD(Value);
Value	fabsD(Value);
Value	floorD(Value);
Value	hypotD(Value, Value);
Value	j0D(Value);
Value	j1D(Value);
Value	jnD(Value,Value);
Value	log10D(Value);
Value	logD(Value);
Value	sinD(Value);
Value	sinhD(Value);
Value	sqrtD(Value);
Value	tanD(Value);
Value	tanhD(Value);
Value	y0D(Value);
Value	y1D(Value);
Value	ynD(Value,Value);
Value	_random(Value);
Value	_srandom(Value);

struct fbuiltin_v funcs_v[] = {
    { doprintf,		"printf" },
    { doscanf,		"scanf" },
    { dofprintf,	"fprintf" },
    { Kill,		"Kill" },
    { Trace,		"Trace" },
    { Trace,		"trace", &DebugScope },
    { doHistoryShow,    "HistoryShow" },
    { 0,		0 },
};
struct fbuiltin_0 funcs_0[] = {
    { Cont,		"Cont" },
    { CurrentThread,	"CurrentThread" },
    { NewMutex,		"NewMutex" },
    { NewSemaphore,	"NewSemaphore" },
    { ThreadsList,	"ThreadsList" },
    { dogetchar,	"getchar" },
    { dotime,		"time" },
    { DebugUp,		"up", &DebugScope, },
    { DebugDown,	"down", &DebugScope },
    { DebugDone,	"done", &DebugScope },
    { 0,		0 },
};
struct fbuiltin_1 funcs_1[] = {
    { Atof,		"atof" },
    { Atoi,		"atoi" },
    { GetPriority,	"GetPriority" },
    { MutexAcquire,	"MutexAcquire" },
    { MutexRelease,	"MutexRelease" },
    { MutexTryAcquire,	"MutexTryAcquire" },
    { SemaphoreSignal,	"SemaphoreSignal" },
    { SemaphoreTest,	"SemaphoreTest" },
    { SemaphoreWait,	"SemaphoreWait" },
    { Sleep,		"sleep" },
    { ThreadFromId,	"ThreadFromId" },
    { ThreadJoin,	"join" },
    { acosD,		"acos" },
    { asinD,		"asin" },
    { atanD,		"atan" },
    { ceilD,		"ceil" },
    { cosD,		"cos" },
    { coshD,		"cosh" },
    { doHistoryInsert,	"HistoryInsert" },
    { dofclose,		"fclose" },
    { dofflush,		"fflush" },
    { dogetc,		"getc" },
    { doputchar,	"putchar" },
    { expD,		"exp" },
    { fabsD,		"abs" },
    { floorD,		"floor" },
    { j0D,		"j0" },
    { j1D,		"j1" },
    { log10D,		"log10" },
    { logD,		"log" },
    { sinD,		"sin" },
    { sinhD,		"sinh" },
    { sqrtD,		"sqrt" },
    { tanD,		"tan" },
    { tanhD,		"tanh" },
    { y0D,		"y0" },
    { y1D,		"y1" },
    { _random,		"random", &PrimitiveScope },
    { _srandom,		"srandom", &PrimitiveScope },
    { 0,		0 },
};
struct fbuiltin_2 funcs_2[] = {
    { SetPriority,	"SetPriority" },
    { Strtol,		"strtol" },
    { atan2D,		"atan2" },
    { dofopen,		"fopen" },
    { dogcd,		"gcd" },
    { doputc,		"putc" },
    { hypotD,		"hypot" },
    { jnD,		"jn" },
    { ynD,		"yn" },
    { 0,		0 },
};
struct fbuiltin_7 funcs_7[] = {
    { doprint,		"Print" },
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
    { BUILD,	"build" },
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
    { 0,	    0 },
};

SymbolPtr
BuiltinSymbol (ScopePtr *scopep,
	       char	*name,
	       Type	type)
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

void
BuiltinAddFunction (ScopePtr *scopep, char *name,
		    int argc, BuiltinFunc f)
{
    ENTER ();
    Value	func;
    SymbolPtr	sym;
    sym = BuiltinSymbol (scopep, name, type_func);
    func =  NewFunc (NewBuiltinCode (type_undef, argc, f), 0);
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
    struct fbuiltin_7	*f_7;
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
    for (f_v = funcs_v; f_v->bf_name; f_v++) {
	f.builtinN = f_v->bf_func;
	BuiltinAddFunction (f_v->bf_scope, f_v->bf_name, -1, f);
    }
    for (f_0 = funcs_0; f_0->bf_name; f_0++) {
	f.builtin0 = f_0->bf_func;
	BuiltinAddFunction (f_0->bf_scope, f_0->bf_name, 0, f);
    }
    for (f_1 = funcs_1; f_1->bf_name; f_1++) {
	f.builtin1 = f_1->bf_func;
	BuiltinAddFunction (f_1->bf_scope, f_1->bf_name, 1, f);
    }
    for (f_2 = funcs_2; f_2->bf_name; f_2++) {
	f.builtin2 = f_2->bf_func;
	BuiltinAddFunction (f_2->bf_scope, f_2->bf_name, 2, f);
    }
    for (f_7 = funcs_7; f_7->bf_name; f_7++) {
	f.builtin7 = f_7->bf_func;
	BuiltinAddFunction (f_7->bf_scope, f_7->bf_name, 7, f);
    }
    for (d = dvars; d->bd_name; d++) {
	sym = ScopeAddSymbol (GlobalScope, 
			      NewSymbolGlobal (AtomId (d->bd_name), 
					       type_double, publish_private));
	sym->global.value->constant = True;
	BoxValue (sym->global.value, 0) = NewDouble (d->bd_value);
    }
    for (s = svars; s->bs_name; s++) {
	sym = BuiltinSymbol (s->bs_scope, s->bs_name, type_string);
	BoxValue (sym->global.value, 0) = NewStrString (s->bs_value);
    }
    for (i = ivars; i->is_name; i++) {
	Value   f;
	
	switch (i->is_file) { 
	case 0: f = FileStdin; break; 
        case 1: f = FileStdout; break;
	default: f = FileStderr;  break;
	}
	sym = BuiltinSymbol (i->is_scope, i->is_name, type_file);
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
    int n = IntPart (bits, "non-integer");
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
    
