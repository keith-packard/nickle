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

struct rbuiltin {
    char	*br_value;
    int		br_prec;
    char	*br_name;
    ScopePtr	*br_scope;
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
    { Imprecise,	"imprecise",	type_float, "n." },
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
    { Atof,		"atof",		type_float,	"s" },
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
    { dim,		"dim",		type_int,	"a" },
    { dims,		"dims",		type_array,	"a" },
    { precision,	"precision",	type_integer,	"n" },
    { doHistoryInsert,	"HistoryInsert",type_undef,	"p" },
    { dofclose,		"fclose",	type_int,	"f" },
    { dofflush,		"fflush",	type_int,	"f" },
    { dogetc,		"getc",		type_int,	"f" },
    { doputchar,	"putchar",	type_int,	"n" },
    { absD,		"abs",		type_float,	"n" },
    { floorD,		"floor",	type_integer,	"n" },
    { lengthS,          "length",	type_int,	"s", &StringScope },
    { _random,		"random",	type_int,	"n", &PrimitiveScope },
    { _srandom,		"srandom",	type_int,	"n", &PrimitiveScope },
    { 0,		0 },
};
struct fbuiltin_2 funcs_2[] = {
    { SetPriority,	"setPriority",	type_int,	"tn", &ThreadScope },
    { Strtol,		"strtol",	type_integer,	"sn" },
    { dofopen,		"fopen",	type_file,	"ss" },
    { dogcd,		"gcd",		type_integer,	"nn" },
    { doputc,		"putc",		type_int,	"nf" },
    { dosetbuf,		"setbuffer",	type_int,	"fn" },
    { indexS,           "index",	type_int,	"ss", &StringScope },
    { 0,		0 },
};
struct fbuiltin_3 funcs_3[] = {
    { substrS,          "substr",	type_string,	"snn", &StringScope },
};
struct fbuiltin_7 funcs_7[] = {
    { doprint,		"Print",	type_int,	"fpsnnns" },
    { 0,		0 },
};

struct fbuiltin_2j funcs_2j[] = {
    { SetJump,		"setjump",	type_undef,	"*cp" },
    { LongJump,		"longjump",	type_undef,	"cp" },
    { 0,		0 },
};

struct rbuiltin rvars[] = {
    { "3."
	"14159265358979323846264338327950288419716939937510"
	"58209749445923078164062862089986280348253421170679"
	"82148086513282306647093844609550582231725359408128"
	"48111745028410270193852110555964462294895493038196"
	"44288109756659334461284756482337867831652712019091"

	"45648566923460348610454326648213393607260249141273"
	"72458700660631558817488152092096282925409171536436"
	"78925903600113305305488204665213841469519415116094"
	"33057270365759591953092186117381932611793105118548"
	"07446237996274956735188575272489122793818301194912"

	"98336733624406566430860213949463952247371907021798"
	"60943702770539217176293176752384674818467669405132"
	"00056812714526356082778577134275778960917363717872"
	"14684409012249534301465495853710507922796892589235"
	"42019956112129021960864034418159813629774771309960"

	"51870721134999999837297804995105973173281609631859"
	"50244594553469083026425223082533446850352619311881"
	"71010003137838752886587533208381420617177669147303"
	"59825349042875546873115956286388235378759375195778"
	"18577805321712268066130019278766111959092164201989"

	"38095257201065485863278865936153381827968230301952"
	"03530185296899577362259941389124972177528347913151"
	"55748572424541506959508295331168617278558890750983"
	"81754637464939319255060400927701671139009848824012"
	"85836160356370766010471018194295559619894676783744"

	"94482553797747268471040475346462080466842590694912"
	"93313677028989152104752162056966024058038150193511"
	"25338243003558764024749647326391419927260426992279"
	"67823547816360093417216412199245863150302861829745"
	"55706749838505494588586926995690927210797509302955",
	4096, "pi"
    },
    { "2."
	"71828182845904523536028747135266249775724709369995"
	"95749669676277240766303535475945713821785251664274"
	"27466391932003059921817413596629043572900334295260"
	"59563073813232862794349076323382988075319525101901"
	"15738341879307021540891499348841675092447614606680"
	
	"82264800168477411853742345442437107539077744992069"
	"55170276183860626133138458300075204493382656029760"
	"67371132007093287091274437470472306969772093101416"
	"92836819025515108657463772111252389784425056953696"
	"77078544996996794686445490598793163688923009879312"
	
	"77361782154249992295763514822082698951936680331825"
	"28869398496465105820939239829488793320362509443117"
	"30123819706841614039701983767932068328237646480429"
	"53118023287825098194558153017567173613320698112509"
	"96181881593041690351598888519345807273866738589422"
	
	"87922849989208680582574927961048419844436346324496"
	"84875602336248270419786232090021609902353043699418"
	"49146314093431738143640546253152096183690888707016"
	"76839642437814059271456354906130310720851038375051"
	"01157477041718986106873969655212671546889570350354"
	
	"02123407849819334321068170121005627880235193033224"
	"74501585390473041995777709350366041699732972508868"
	"76966403555707162268447162560798826517871341951246"
	"65201030592123667719432527867539855894489697096409"
	"75459185695638023637016211204774272283648961342251"
	
	"64450781824423529486363721417402388934412479635743"
	"70263755294448337998016125492278509257782562092622"
	"64832627793338656648162772516401910590049164499828"
	"93150566047258027786318641551956532442586982946959"
	"30801915298721172556347546396447910145904090586298"
	
	"49679128740687050489585867174798546677575732056812"
	"88459205413340539220001137863009455606881667400169"
	"84205580403363795376452030402432256613527836951177"
	"88386387443966253224985065499588623428189970773327"
	"61717839280349465014345588970719425863987727547109"
	
	"62953741521115136835062752602326484728703920764310"
	"05958411661205452970302364725492966693811513732275"
	"36450988890313602057248176585118063036442812314965"
	"50704751025446501172721155519486685080036853228183"
	"15219600373562527944951582841882947876108526398139"
	
	"55990067376482922443752871846245780361929819713991"
	"47564488262603903381441823262515097482798777996437"
	"30899703888677822713836057729788241256119071766394"
	"65070633045279546618550966661856647097113444740160"
	"70462621568071748187784437143698821855967095910259"
	
	"68620023537185887485696522000503117343920732113908"
	"03293634479727355955277349071783793421637012050054"
	"51326383544000186323991490705479778056697853358048"
	"96690629511943247309958765523681285904138324116072"
	"26029983305335",
	4096, "e" 
    },
    { 0,			    0, 0 },
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

static SymbolPtr
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

static SymbolPtr
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

static ArgType *
BuiltinArgTypes (char *format, int *argcp)
{
    ENTER ();
    ArgType	*args, *a, **last;
    int		argc;
    Types	*t;
    Bool	ref;
    
    args = 0;
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
	default:
	case 'p': t = NewTypesPrim (type_undef); break;
	case 'n': t = NewTypesPrim (type_float); break;
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

static void
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

static void
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
    struct rbuiltin	*r;
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
    
    for (r = rvars; r->br_name; r++) {
	sym = ScopeAddSymbol (GlobalScope, 
			      NewSymbolGlobal (AtomId (r->br_name), 
					       NewTypesPrim (type_float), 
					       publish_private));
	sym->global.value->constant = True;
	BoxValue (sym->global.value, 0) = NewValueFloat (aetov (r->br_value),
							 r->br_prec);
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
Imprecise (int n, Value *p)
{
    ENTER();
    Value   v;
    int	    prec;

    if (n == 0)
    {
	RaiseError ("imprecise: takes at least one argument");
	RETURN(Zero);
    }
    v = p[0];
    if (n > 1)
	prec = IntPart (p[1], "imprecise: invalid precision");
    else
	prec = DEFAULT_FLOAT_PREC;

    RETURN (NewValueFloat (v, prec));
}

Value 
absD (Value a)
{
    ENTER ();
    if (Negativep (a))
	a = Negate (a);
    RETURN (a);
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

Value
precision (Value av)
{
    ENTER ();
    unsigned	prec;

    if (!Numericp (av->value.tag))
    {
	RaiseError ("precision: argument non-numeric %v", av);
	RETURN (Zero);
    }
    if (av->value.tag == type_float)
	prec = av->floats.prec;
    else
	prec = 0;
    RETURN (NewInt (prec));
}
