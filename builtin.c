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

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"nickle.h"
#include	"gram.h"

NamespacePtr    DebugNamespace;
NamespacePtr    FileNamespace;
NamespacePtr    HistoryNamespace;
NamespacePtr    MathNamespace;
#ifdef BSD_RANDOM
NamespacePtr    BSDRandomNamespace;
#endif
NamespacePtr    SemaphoreNamespace;
NamespacePtr    StringNamespace;
NamespacePtr    ThreadNamespace;
NamespacePtr	CommandNamespace;
#ifdef GCD_DEBUG
NamespacePtr	GcdNamespace;
#endif
NamespacePtr	EnvironNamespace;

struct fbuiltin_v {
    Value	    (*func) (int, Value *);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_0 {
    Value	    (*func) (void);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_1 {
    Value	    (*func) (Value);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_2 {
    Value	    (*func) (Value, Value);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_3 {
    Value	    (*func) (Value, Value, Value);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_4 {
    Value	    (*func) (Value, Value, Value, Value);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_5 {
    Value	    (*func) (Value, Value, Value, Value, Value);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_6 {
    Value	    (*func) (Value, Value, Value, Value, Value, Value);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_7 {
    Value	    (*func) (Value, Value, Value, Value, Value, Value, Value);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_vj {
    Value	    (*func) (InstPtr *, int, Value *);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_0j {
    Value	    (*func) (InstPtr *);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_1j {
    Value	    (*func) (InstPtr *, Value);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct fbuiltin_2j {
    Value	    (*func) (InstPtr *, Value, Value);
    char	    *name;
    char	    *ret;
    char	    *args;
    NamespacePtr    *namespace;
};

struct rbuiltin {
    char	    *value;
    int	    	    prec;
    char	    *name;
    NamespacePtr    *namespace;
};

struct sbuiltin {
    char	    *value;
    char	    *name;
    NamespacePtr    *namespace;
};

struct ibuiltin {
    char	    *name;
    int	    	    file;
    NamespacePtr    *namespace;
};

struct nbuiltin {
    char	    *name;
    NamespacePtr    *value;
    NamespacePtr    *namespace;
};

struct ebuiltin {
    char		*name;
    StandardException	exception;
    char		*args;
    NamespacePtr	*namespace;
};

static struct fbuiltin_v funcs_v[] = {
    { do_printf,	    "printf",		    "i",    "s.p"   },
    { do_fprintf,	    "fprintf",		    "i",    "fs.p", &FileNamespace},
    { do_imprecise,	    "imprecise",	    "R",    "R.i"   },
    { do_Thread_kill,	    "kill",		    "i",    ".t",   &ThreadNamespace },
    { do_Thread_trace,	    "trace",		    "i",    ".p",   &ThreadNamespace },
    { do_Thread_trace,	    "trace",		    "i",    ".p",   &DebugNamespace },
    { do_History_show,	    "show",		    "i",    "s.i",  &HistoryNamespace },
    { do_string_to_integer, "string_to_integer",    "i",    "s.i"   },
    { do_Semaphore_new,	    "new",		    "S",    ".i",   &SemaphoreNamespace },
    { do_Command_undefine,  "undefine",		    "i",    ".A*s", &CommandNamespace },
    { 0,		    0 },
};

static struct fbuiltin_0 funcs_0[] = {
    { do_Thread_cont,	    "cont",		    "i",    "",	    &ThreadNamespace },
    { do_Thread_current,    "current",		    "t",    "",	    &ThreadNamespace },
    { do_Thread_list,	    "list",		    "i",    "",	    &ThreadNamespace },
    { do_getchar,	    "getchar",		    "i",    ""	    },
    { do_time,		    "time",		    "i",    ""	    },
    { do_File_string_write, "string_write",	    "f",    "",	    &FileNamespace },
    { do_Debug_up,	    "up",		    "i",    "",	    &DebugNamespace, },
    { do_Debug_down,	    "down",		    "i",    "",	    &DebugNamespace },
    { do_Debug_done,	    "done",		    "i",    "",	    &DebugNamespace },
    { do_Debug_collect,	    "collect",		    "i",    "",	    &DebugNamespace },
    { 0,		    0 },
};

static struct fbuiltin_1 funcs_1[] = {
    { do_putchar,	    "putchar",		    "i",    "i"	    },
    { do_sleep,		    "sleep",		    "i",    "i"	    },
    { do_exit,		    "exit",		    "v",    "i"	    },
    { do_dim,		    "dim",		    "i",    "A*p"   },
    { do_dims,		    "dims",		    "A*i",  "Ap"    },
    { do_reference,	    "reference",	    "*p",   "p"	    },
    { do_string_to_real,    "string_to_real",	    "R",    "s"	    },
    { do_abs,		    "abs",		    "n",    "R"	    },
    { do_floor,		    "floor",		    "i",    "R"	    },
    { do_ceil,		    "ceil",		    "i",    "R"	    },
    { do_exponent,	    "exponent",		    "i",    "R"	    },
    { do_mantissa,	    "mantissa",		    "r",    "R"	    },
    { do_numerator,	    "numerator",	    "i",    "R"	    },
    { do_denominator,	    "denominator",	    "i",    "R"	    },
    { do_precision,	    "precision",	    "i",    "R"	    },
    { do_sign,		    "sign",		    "i",    "R"	    },
    { do_bit_width,	    "bit_width",	    "i",    "i"	    },
    { do_is_int,	    "is_int",		    "i",    "p"	    },
    { do_is_rational,	    "is_rational",	    "i",    "p"	    },
    { do_is_number,	    "is_number",	    "i",    "p"	    },
    { do_is_string,	    "is_string",	    "i",    "p"	    },
    { do_is_file,	    "is_file",		    "i",    "p"	    },
    { do_is_thread,	    "is_thread",    	    "i",    "p"	    },
    { do_is_semaphore,	    "is_semaphore",    	    "i",    "p"	    },
    { do_is_continuation,   "is_continuation",	    "i",    "p"	    },
    { do_is_array,	    "is_array",		    "i",    "p"	    },
    { do_is_ref,	    "is_ref",		    "i",    "p"	    },
    { do_is_struct,	    "is_struct",	    "i",    "p"	    },
    { do_is_func,	    "is_func",		    "i",    "p"	    },
    { do_is_void,	    "is_void",		    "i",    "p"	    },
    { do_Thread_get_priority,"get_priority",	    "i",    "t",    &ThreadNamespace },
    { do_Thread_id_to_thread,"id_to_thread",	    "t",    "i",    &ThreadNamespace },
    { do_Thread_join,	    "join",		    "p",    "t",    &ThreadNamespace },
    { do_Semaphore_signal,  "signal",		    "i",    "S",    &SemaphoreNamespace },
    { do_Semaphore_wait,    "wait",		    "i",    "S",    &SemaphoreNamespace },
    { do_Semaphore_test,    "test",		    "i",    "S",    &SemaphoreNamespace },
    { do_History_insert,    "insert",		    "p",    "p",    &HistoryNamespace },
    { do_File_close,	    "close",		    "i",    "f",    &FileNamespace },
    { do_File_flush,	    "flush",		    "i",    "f",    &FileNamespace },
    { do_File_getc,	    "getc",		    "i",    "f",    &FileNamespace },
    { do_File_end,	    "end",		    "i",    "f",    &FileNamespace },
    { do_File_error,	    "error",		    "i",    "f",    &FileNamespace },
    { do_File_clear_error,  "clear_error",	    "i",    "f",    &FileNamespace },
    { do_File_string_read,  "string_read",	    "f",    "s",    &FileNamespace },
    { do_File_string_string,"string_string",	    "s",    "f",    &FileNamespace },
    { do_String_length,	    "length",		    "i",    "s",    &StringNamespace },
    { do_String_new,	    "new",		    "s",    "p",    &StringNamespace },
#ifdef BSD_RANDOM
    { do_BSD_random,	    "random",		    "i",    "i",    &BSDRandomNamespace },
    { do_BSD_srandom,	    "srandom",		    "i",    "i",    &BSDRandomNamespace },
#endif
    { do_Debug_dump,	    "dump",		    "i",    "p",    &DebugNamespace },
    { do_Command_delete,    "delete",		    "i",    "s",    &CommandNamespace },
    { do_Command_lex_file,  "lex_file",		    "i",    "s",    &CommandNamespace },
    { do_Command_lex_string,"lex_string",	    "i",    "s",    &CommandNamespace },
    { do_Command_edit,	    "edit",		    "i",    "A*s",  &CommandNamespace },
    { do_Environ_get,	    "get",		    "p",    "s",    &EnvironNamespace },
    { do_Environ_unset,	    "unset",		    "i",    "s",    &EnvironNamespace },
    { 0,		    0 },
};

static struct fbuiltin_2 funcs_2[] = {
    { do_Thread_set_priority,"set_priority",	    "i",    "ti",   &ThreadNamespace },
    { do_File_open,	    "open",		    "f",    "ss",   &FileNamespace },
    { do_gcd,		    "gcd",		    "i",    "ii"    },
    { do_xor,		    "xor",		    "i",    "ii"    },
    { do_Math_pow,	    "pow",		    "n",    "Ri",   &MathNamespace },
    { do_Math_assignpow,    "assign_pow",	    "n",    "*Ri",  &MathNamespace },
    { do_File_putc,	    "putc",		    "i",    "if",   &FileNamespace },
    { do_File_ungetc,	    "ungetc",		    "i",    "if",   &FileNamespace },
    { do_File_setbuf,	    "setbuffer",	    "i",    "fi",   &FileNamespace },
    { do_String_index,      "index",		    "i",    "ss",   &StringNamespace },
    { do_setjmp,	    "setjmp",		    "p",    "*cp"   },
    { do_Command_new,	    "new",		    "i",    "sp",   &CommandNamespace },
    { do_Command_new_names, "new_names",    	    "i",    "sp",   &CommandNamespace },
    { do_Command_pretty_print,"pretty_print",	    "i",    "fA*s", &CommandNamespace },
    { do_Command_display,   "display",		    "i",    "sp",   &CommandNamespace },
#ifdef GCD_DEBUG
    { do_Gcd_bdivmod,	    "bdivmod",		    "i",    "ii",   &GcdNamespace},
    { do_Gcd_kary_reduction,"kary_reduction",	    "i",    "ii",   &GcdNamespace},
#endif
    { do_Environ_set,	    "set",		    "s",    "ss",   &EnvironNamespace },
    { 0,		    0 },
};

static struct fbuiltin_3 funcs_3[] = {
    { do_String_substr,	    "substr",		    "s",    "sii",  &StringNamespace },
    { do_File_pipe,	    "pipe",		    "f",    "sA*ss", &FileNamespace },
    { 0,		    0 },
};

static struct fbuiltin_7 funcs_7[] = {
    { do_File_print,	    "print",		    "i",    "fpsiiis",&FileNamespace },
    { 0,		    0 },
};

static struct fbuiltin_2j funcs_2j[] = {
    { do_longjmp,	    "longjmp",		    "p",    "cp"    },
    { 0,		0 },
};

static struct rbuiltin rvars[] = {
    { "3."
	"14159265358979323846264338327950288419716939937510"
	"58209749445923078164062862089986280348253421170679"
#if 0
	/* only need around 100 digits to get 256 bits */
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
	"55706749838505494588586926995690927210797509302955"
#endif
	, 256, "pi"
    },
    { "2."
	"71828182845904523536028747135266249775724709369995"
	"95749669676277240766303535475945713821785251664274"
#if 0
	/* only need around 100 digits to get 256 bits */
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
	"26029983305335"
#endif
	, 256, "e" 
    },
    { 0,			    0, 0 },
};

static struct sbuiltin svars[] = {
    { "> ",	    "prompt" },
    { "+ ",	    "prompt2" },
    { "- ",	    "prompt3" },
    { "%g",	    "format" },
    { VERSION,	    "version" },
#ifdef BUILD
    { BUILD,	    "build" },
#else
    { "?",	    "build" },
#endif
    { 0,    0 },
};

static struct ibuiltin ivars[] = {
    { "stdin",	0 },
    { "stdout",	1 },
    { "stderr",	2 },
    { 0,	0 },
};

static struct nbuiltin nvars[] = {
    { "Debug",	    &DebugNamespace },
    { "File",	    &FileNamespace },
    { "History",    &HistoryNamespace },
    { "Math",	    &MathNamespace },
#ifdef BSD_RANDOM
    { "BSDRandom",  &BSDRandomNamespace },
#endif
    { "Semaphore",  &SemaphoreNamespace },
    { "String",	    &StringNamespace },
    { "Thread",	    &ThreadNamespace },
    { "Command",    &CommandNamespace },
#ifdef GCD_DEBUG
    { "Gcd",	    &GcdNamespace },
#endif
    { "Environ",    &EnvironNamespace },
    { 0,	    0 },
};

static struct ebuiltin excepts[] = {
    {"uninitialized_value",	exception_uninitialized_value,	"s" },
    {"invalid_argument",	exception_invalid_argument,	"sip" },
    {"readonly_box",		exception_readonly_box,		"sp" },
    {"invalid_array_bounds",	exception_invalid_array_bounds,	"spp" },
    {"divide_by_zero",		exception_divide_by_zero,	"snn" },
    {"invalid_struct_member",	exception_invalid_struct_member,"sps" },
    {"invalid_binop_values",	exception_invalid_binop_values,	"spp" },
    {"invalid_unop_value",	exception_invalid_unop_value,	"sp" },
    {0,				0 },
};

static SymbolPtr
BuiltinSymbol (NamespacePtr *namespacep,
	       char	    *name,
	       Types	    *type)
{
    ENTER ();
    NamespacePtr    namespace;

    if (namespacep)
	namespace = *namespacep;
    else
	namespace = GlobalNamespace;
    RETURN (NamespaceAddSymbol (namespace,
			    NewSymbolGlobal (AtomId (name), type,
					     publish_public)));
}

static SymbolPtr
BuiltinNamespace (NamespacePtr  *namespacep,
		  char		*name)
{
    ENTER ();
    NamespacePtr	namespace;
    SymbolPtr		sym;

    if (namespacep)
	namespace = *namespacep;
    else
	namespace = GlobalNamespace;
    sym = NewSymbolNamespace (AtomId (name), publish_public);
    sym->namespace.namespace = NewNamespace (namespace);
    RETURN (NamespaceAddSymbol (namespace, sym));
}

static SymbolPtr
BuiltinException (NamespacePtr  *namespacep,
		  char		*name,
		  Types		*type)
{
    ENTER ();
    NamespacePtr	namespace;
    SymbolPtr		sym;

    if (namespacep)
	namespace = *namespacep;
    else
	namespace = GlobalNamespace;
    sym = NewSymbolException (AtomId (name), type, publish_public);
    RETURN (NamespaceAddSymbol (namespace, sym));
}

static char *
BuiltinType (char *format, Types **type)
{
    Types   *t;
    Bool    ref = False;
    Bool    array = False;
    Expr    *dims = 0;
    
    ref = False;
    if (*format == '*')
    {
	ref = True;
	format++;
    }
    if (*format == 'A')
    {
	array = True;
	format++;
	while (*format == '*')
	{
	    dims = NewExprTree (COMMA, 0, dims);
	    format++;
	}
    }
    switch (*format++) {
    case 'p': t = typesPoly; break;
    case 'n': t = typesGroup; break;
    case 'N': t = typesField; break;
    case 'R': t = typesPrim[type_float]; break;
    case 'r': t = typesPrim[type_rational]; break;
    case 'i': t = typesPrim[type_integer]; break;
    case 's': t = typesPrim[type_string]; break;
    case 'f': t = typesPrim[type_file]; break;
    case 't': t = typesPrim[type_thread]; break;
    case 'S': t = typesPrim[type_semaphore]; break;
    case 'c': t = typesPrim[type_continuation]; break;
    case 'v': t = typesPrim[type_void]; break;
    default: 
	t = 0;
	write (2, "Invalid builtin argument type\n", 30);
	break;
    }
    if (ref)
	t = NewTypesRef (t);
    if (array)
	t = NewTypesArray (t, dims);
    *type = t;
    return format;
}
	     
static ArgType *
BuiltinArgTypes (char *format, int *argcp)
{
    ENTER ();
    ArgType	*args, *a, **last;
    int		argc;
    Types	*t;
    Bool	varargs;
    
    args = 0;
    last = &args;
    argc = 0;
    while (*format)
    {
	varargs = False;
	if (*format == '.')
	{
	    varargs = True;
	    format++;
	}
	format = BuiltinType (format, &t);
	if (!varargs)
	    argc++;
        a = NewArgType (t, varargs, 0, 0);
	*last = a;
	last = &a->next;
    }
    *argcp = argc;
    RETURN(args);
}

static void
BuiltinAddFunction (NamespacePtr *namespacep, char *name, char *ret_format,
		    char *format, BuiltinFunc f)
{
    ENTER ();
    Value	func;
    SymbolPtr	sym;
    int		argc;
    ArgType	*args;
    Types	*ret;

    args = BuiltinArgTypes (format, &argc);
    BuiltinType (ret_format, &ret);
    sym = BuiltinSymbol (namespacep, name, NewTypesFunc (ret, args));
    func =  NewFunc (NewBuiltinCode (ret, args, argc, f, False), 0);
    BoxValueSet (sym->global.value, 0, func);
    EXIT ();
}

static void
BuiltinAddJumpingFunction (NamespacePtr *namespacep, char *name, char *ret_format,
			   char *format, BuiltinFunc f)
{
    ENTER ();
    Value	func;
    SymbolPtr	sym;
    ArgType	*args;
    int		argc;
    Types	*ret;
    
    args = BuiltinArgTypes (format, &argc);
    BuiltinType (ret_format, &ret);
    sym = BuiltinSymbol (namespacep, name, NewTypesFunc (ret, args));
    func =  NewFunc (NewBuiltinCode (ret, args, argc, f, True), 0);
    BoxValueSet (sym->global.value, 0, func);
    EXIT ();
}

static void
BuiltinAddException (NamespacePtr	*namespacep, 
		     StandardException	exception,
		     char		*name,
		     char		*format)
{
    ENTER ();
    SymbolPtr	sym;
    ArgType	*args;
    Types	*type;
    int		argc;

    args = BuiltinArgTypes (format, &argc);
    type = NewTypesFunc (typesPoly, args);
    sym = BuiltinException (namespacep, name, NewTypesFunc (typesPoly, args));
    RegisterStandardException (exception, sym);
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
    struct ebuiltin	*e;
    BuiltinFunc		f;
    SymbolPtr		sym;

    for (n = nvars; n->name; n++) {
	sym = BuiltinNamespace (n->namespace, n->name);
	*n->value = sym->namespace.namespace;
    }
    for (f_v = funcs_v; f_v->name; f_v++) {
	f.builtinN = f_v->func;
	
	BuiltinAddFunction (f_v->namespace, f_v->name, f_v->ret, f_v->args, f);
    }
    for (f_0 = funcs_0; f_0->name; f_0++) {
	f.builtin0 = f_0->func;
	BuiltinAddFunction (f_0->namespace, f_0->name, f_0->ret, f_0->args, f);
    }
    for (f_1 = funcs_1; f_1->name; f_1++) {
	f.builtin1 = f_1->func;
	BuiltinAddFunction (f_1->namespace, f_1->name, f_1->ret, f_1->args, f);
    }
    for (f_2 = funcs_2; f_2->name; f_2++) {
	f.builtin2 = f_2->func;
	BuiltinAddFunction (f_2->namespace, f_2->name, f_2->ret, f_2->args, f);
    }
    for (f_3 = funcs_3; f_3->name; f_3++) {
	f.builtin3 = f_3->func;
	BuiltinAddFunction (f_3->namespace, f_3->name, f_3->ret, f_3->args, f);
    }
    for (f_7 = funcs_7; f_7->name; f_7++) {
	f.builtin7 = f_7->func;
	BuiltinAddFunction (f_7->namespace, f_7->name, f_7->ret, f_7->args, f);
    }
    for (f_2j = funcs_2j; f_2j->name; f_2j++) {
	f.builtin2J = f_2j->func;
	BuiltinAddJumpingFunction (f_2j->namespace, f_2j->name, f_2j->ret, f_2j->args, f);
    }
    
    for (r = rvars; r->name; r++) {
	sym = NamespaceAddSymbol (GlobalNamespace, 
			      NewSymbolGlobal (AtomId (r->name), 
					       typesPrim[type_float], 
					       publish_private));
	sym->global.value->constant = True;
	BoxValueSet (sym->global.value, 0,
		     NewValueFloat (aetov (r->value),r->prec));
    }
    for (s = svars; s->name; s++) {
	sym = BuiltinSymbol (s->namespace, s->name, typesPrim[type_string]);
	BoxValueSet (sym->global.value, 0, NewStrString (s->value));
    }
    for (i = ivars; i->name; i++) {
	Value   f;
	
	switch (i->file) { 
	case 0: f = FileStdin; break; 
        case 1: f = FileStdout; break;
	default: f = FileStderr;  break;
	}
	sym = BuiltinSymbol (i->namespace, i->name, typesPrim[type_file]);
	BoxValueSet (sym->global.value, 0, f);
    }
    
    for (e = excepts; e->name; e++)
	BuiltinAddException (e->namespace, e->exception, e->name, e->args);
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
do_printf (int n, Value *p)
{
    char	*fmt;
    
    fmt = StringChars (&p[0]->string);
    p++;
    n--;
    callformat (FileStdout, fmt, n, p);
    return Zero;
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
do_File_status (Value f)
{
    ENTER ();
    RETURN (NewInt (FileStatus (f)));
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
do_getchar ()
{
    ENTER ();
    RETURN (do_File_getc (FileStdin));
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
do_putchar (Value v)
{
    ENTER ();
    RETURN (do_File_putc (v, FileStdout));
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
do_gcd (Value a, Value b)
{
    ENTER ();
    RETURN (Gcd (a, b));
}

#ifdef GCD_DEBUG
Value 
do_Gcd_bdivmod (Value a, Value b)
{
    ENTER ();
    RETURN (Bdivmod (a, b));
}

Value 
do_Gcd_kary_reduction (Value a, Value b)
{
    ENTER ();
    RETURN (KaryReduction (a, b));
}
#endif

Value
do_xor (Value a, Value b)
{
    ENTER ();
    RETURN (Lxor (a, b));
}

Value
do_Math_pow (Value a, Value b)
{
    ENTER ();
    RETURN (Pow (a, b));
}

Value
do_Math_assignpow (Value a, Value b)
{
    ENTER ();
    Value   ret;
    
    ret = Pow (RefValue (a), b);
    RefValueSet (a, ret);
    RETURN (ret);
}

Value
do_time (void)
{
    ENTER ();
    RETURN (NewInt (time (0)));
}

Value
do_History_insert (Value a)
{
    ENTER ();
    HistoryInsert (a);
    complete = True;
    RETURN (a);
}

Value
do_History_show (int n, Value *p)
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

extern Value	atov (char *, int);
extern Value	aetov (char *);

Value
do_string_to_integer (int n, Value *p)
{
    ENTER ();
    char    *s;
    int	    ibase;
    int	    negative = 0;
    Value   ret = Zero;
    Value   str = p[0];
    Value   base = Zero;
    
    switch(n) {
    case 1:
	break;
    case 2:
	base = p[1];
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"string_to_integer: wrong number of arguments",
				2,
				NewInt (2),
				NewInt (n));
	RETURN(Zero);
    }
    
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
    ibase = IntPart (base, "string_to_integer: invalid base");
    if (!aborting)
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
	if (!aborting)
	{
	    if (negative)
		ret = Negate (ret);
	}
    }
    RETURN (ret);
}

Value
do_string_to_real (Value str)
{
    ENTER ();
    RETURN (aetov (StringChars (&str->string)));
}


Value
do_imprecise (int n, Value *p)
{
    ENTER();
    Value   v;
    int	    prec;

    v = p[0];
    if (n > 1)
	prec = IntPart (p[1], "imprecise: invalid precision");
    else
    {
	if (v->value.tag == type_float)
	    RETURN(v);
	prec = DEFAULT_FLOAT_PREC;
    }

    RETURN (NewValueFloat (v, prec));
}

Value 
do_abs (Value a)
{
    ENTER ();
    if (Negativep (a))
	a = Negate (a);
    RETURN (a);
}

Value 
do_floor (Value a)
{
    return Floor (a);
}

Value 
do_ceil (Value a)
{
    return Ceil (a);
}

#ifdef BSD_RANDOM

Value
do_BSD_random (Value bits)
{
    ENTER();
    int n = IntPart (bits, "random: modulus non-integer");
    Value ret = Zero;

    if (n > 31)
	RaiseStandardException (exception_invalid_argument,
				"random: modulus exceeds 2^31",
				2, 
				NewInt (0), bits);
    else if (n <= 0)
	RaiseStandardException (exception_invalid_argument,
				"random: bad modulus",
				1, NewInt (0), bits);
    else
	ret = NewInt (random () & ((1 << n) - 1));
    RETURN (ret);
}

Value
do_BSD_srandom (Value seed)
{
    ENTER();
    int n = IntPart (seed, "srandom: non-integer seed");

    srandom ((unsigned int) n);
    RETURN (Zero);
}

#endif

Value
do_String_length (Value av)
{
    ENTER();
    Value ret;
    ret = NewInt(strlen(StringChars(&av->string)));
    RETURN (ret);
}

Value
do_String_new (Value av)
{
    ENTER ();
    Value   ret;
    int	    len, i;
    char    *s;

    if (av->value.tag == type_array && av->array.ndim == 1)
    {
	len = av->array.dim[0];
	ret = NewString (len);
	s = StringChars (&ret->string);
	for (i = 0; i < len; i++)
	    *s++ = IntPart (BoxValue (av->array.values, i),
			    "new: array element not integer");
	*s++ = '\0';
    }
    else
    {
	len = 1;
	ret = NewString (len);
	s = StringChars (&ret->string);
	s[0] = IntPart (av, "new: argument not integer");
	s[1] = '\0';
    }
    RETURN (ret);
}
	       
Value
do_String_index (Value av, Value bv)
{
    ENTER();
    char *a, *b, *p;
    Value ret;
    a = StringChars(&av->string);
    b = StringChars(&bv->string);
    p = strstr(a, b);
    if (!p)
	RETURN (NewInt(-1));
    ret = NewInt(p - a);
    RETURN (ret);
}

Value
do_String_substr (Value av, Value bv, Value cv)
{
    ENTER();
    char *a, *rchars;
    int b, c, al;
    Value ret;
    a = StringChars(&av->string);
    al = strlen(a);
    b = IntPart(bv, "substr: index not integer");
    if (b < 0 || al < b)
    {
	RaiseStandardException (exception_invalid_argument,
				"substr: index out of range",
				2, NewInt (1), bv);
	RETURN (av);
    }
    c = IntPart(cv, "substr: count not integer");
    if (c < 0)
	c = al - b;
    if (b + al < c)
    {
	RaiseStandardException (exception_invalid_argument,
				"substr: count out of range",
				2, NewInt (2), cv);
	RETURN (av);
    }
    ret = NewString(c);
    rchars = StringChars(&ret->string);
    strncpy(rchars, a + b, c);
    rchars[c] = '\0';
    RETURN (ret);
}

Value
do_exit (Value av)
{
    ENTER ();
    int	    code;

    code = IntPart (av, "Illegal exit code");
    if (aborting)
	RETURN (Zero);
    exit (code);
    RETURN (Zero);
}

Value
do_dim(Value av) 
{
    ENTER();
    Value ret;
    if (av->array.ndim != 1)
    {
	RaiseStandardException (exception_invalid_argument,
				"dim: argument must be one-dimensional array",
				2, NewInt (0), av);
	RETURN (Zero);
    }
    ret = NewInt(av->array.dim[0]);
    RETURN (ret);
}

Value
do_dims(Value av) 
{
    ENTER();
    Value ret;
    int i;

    ret = NewArray(True, type_int, 1, &av->array.ndim);
    for (i = 0; i < av->array.ndim; i++) {
      Value d = NewInt(av->array.dim[i]);
      BoxValueSet(ret->array.values, i, d);
    }
    RETURN (ret);
}

Value
do_reference (Value av)
{
    ENTER ();
    Value   ret;

    ret = NewRef (NewBox (False, False, 1), 0);
    RefValueSet (ret, av);
    RETURN (ret);
}

Value
do_precision (Value av)
{
    ENTER ();
    unsigned	prec;

    if (av->value.tag == type_float)
	prec = av->floats.prec;
    else
	prec = 0;
    RETURN (NewInt (prec));
}

Value
do_sign (Value av)
{
    ENTER ();

    if (Zerop (av))
	av = Zero;
    else if (Negativep (av))
	av = NewInt(-1);
    else
	av = One;
    RETURN (av);
}

Value
do_exponent (Value av)
{
    ENTER ();
    Value   ret;

    if (av->value.tag != type_float)
    {
	RaiseStandardException (exception_invalid_argument,
				"exponent: argument must be imprecise",
				2, NewInt (0), av);
	RETURN (Zero);
    }
    ret = NewInteger (av->floats.exp->sign, av->floats.exp->mag);
    ret = Plus (ret, NewInt (FpartLength (av->floats.mant)));
    RETURN (ret);
}

Value
do_mantissa (Value av)
{
    ENTER ();
    Value   ret;

    if (av->value.tag != type_float)
    {
	RaiseStandardException (exception_invalid_argument,
				"mantissa: argument must be imprecise",
				2, NewInt (0), av);
	RETURN (Zero);
    }
    ret = NewInteger (av->floats.mant->sign, av->floats.mant->mag);
    ret = Divide (ret, Pow (NewInt (2), 
			    NewInt (FpartLength (av->floats.mant))));
    RETURN (ret);
}

Value
do_numerator (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_int:
    case type_integer:
	break;
    case type_rational:
	av = NewInteger (av->rational.sign, av->rational.num);
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"numerator: argument must be precise",
				2, NewInt (0), av);
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_denominator (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_int:
    case type_integer:
	av = One;
	break;
    case type_rational:
	av = NewInteger (Positive, av->rational.den);
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"denominator: argument must be precise",
				2, NewInt (0), av);
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_bit_width (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_int:
	av = NewInt (IntWidth (av->ints.value));
	break;
    case type_integer:
	av = NewInt (NaturalWidth (av->integer.mag));
	break;
    default:
	RaiseStandardException (exception_invalid_argument,
				"bit_width: argument must be integer",
				2, NewInt (0), av);
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_int (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_int:
    case type_integer:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_rational (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_int:
    case type_integer:
    case type_rational:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_number (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_int:
    case type_integer:
    case type_rational:
    case type_float:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_string (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_string:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_file (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_file:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_thread (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_thread:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_semaphore (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_semaphore:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_continuation (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_continuation:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_void (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_void:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_array (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_array:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_ref (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_ref:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_struct (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_struct:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_is_func (Value av)
{
    ENTER ();
    switch (av->value.tag) {
    case type_func:
	av = One;
	break;
    default:
	av = Zero;
	break;
    }
    RETURN (av);
}

Value
do_Debug_collect (void)
{
    ENTER ();
    MemCollect ();
    RETURN (One);
}

static Value
do_Command_new_common (Value name, Value func, Bool names)
{
    ENTER();
    char	*cmd;
    int		c;
    
    if (func->value.tag != type_func)
    {
	RaiseStandardException (exception_invalid_argument,
				"argument must be func",
				2, NewInt (1), func);
	RETURN (Zero);
    }
    cmd = StringChars (&name->string);
    while ((c = *cmd++))
    {
	if (isupper (c) || islower (c))
	    continue;
	if (cmd != StringChars (&name->string) + 1)
	{
	    if (isdigit (c) || c == '_')
	     continue;
	}
	RaiseStandardException (exception_invalid_argument,
				"argument must be valid name",
				2, NewInt (0), name);
	RETURN (Zero);
    }
    CurrentCommands = NewCommand (CurrentCommands, 
				  AtomId (StringChars (&name->string)),
				  func, names);
    RETURN (One);
}

Value
do_Command_new (Value name, Value func)
{
    return do_Command_new_common (name, func, False);
}

Value
do_Command_new_names (Value name, Value func)
{
    return do_Command_new_common (name, func, True);
}

Value
do_Command_delete (Value name)
{
    ENTER();
    Atom    id;

    id = AtomId (StringChars (&name->string));
    if (!CommandFind (CurrentCommands, id))
	RETURN (Zero);
    CurrentCommands = CommandRemove (CurrentCommands, id);
    RETURN (One);
}

Value
do_Command_lex_file (Value name)
{
    ENTER ();
    Value   r;

    if (LexFile (StringChars (&name->string), False))
	r = One;
    else
	r = Zero;
    RETURN (r);
}

Value
do_Command_lex_string (Value name)
{
    ENTER ();

    LexString (StringChars (&name->string));
    RETURN (One);
}

Value
do_Command_pretty_print (Value f, Value names)
{
    ENTER();
    NamespacePtr    s;
    SymbolPtr	    sym;

    if (NamespaceLocate (names, &s, &sym) && sym)
	PrettyPrint (f, sym); 
    RETURN (One);
}

Value
do_Command_display (Value format, Value v)
{
    ENTER ();
    if (v != Void)
    {
	Value	a[2];
	a[0] = format;
	a[1] = v;
	do_printf (2, a);
	FileOutput (FileStdout, '\n');
    }
    RETURN (Zero);
}

Value
do_Command_undefine (int argc, Value *args)
{
    ENTER ();
    NamespacePtr    s;
    SymbolPtr	    sym;
    int		    i;
    
    for (i = 0; i < argc; i++)
    {
	if (NamespaceLocate (args[i], &s, &sym) && sym)
	{
	    CurrentTypespace = TypespaceRemove (CurrentTypespace,
						sym->symbol.name);
	    NamespaceRemoveSymbol (GlobalNamespace, sym);
	}
    }
    RETURN (One);
}

Value
do_Command_edit (Value names)
{
    ENTER();
    NamespacePtr    s;
    SymbolPtr	    sym;

    if (NamespaceLocate (names, &s, &sym) && sym)
	EditFunction (sym); 
    RETURN (One);
}

Value
do_Environ_get (Value av)
{
    ENTER ();
    char    *c;

    c = getenv (StringChars (&av->string));
    if (!c)
	RETURN (Zero);
    RETURN (NewStrString (c));
}

Value
do_Environ_unset (Value av)
{
    unsetenv (StringChars (&av->string));
    return One;
}

Value
do_Environ_set (Value name, Value value)
{
    ENTER ();
    if (setenv (StringChars (&name->string), StringChars (&value->string), 1) < 0)
	RETURN (Zero);
    RETURN (One);
}
