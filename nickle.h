/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#if LOCAL_BUILD
#include	"nickle-config.h"
#include	"mem.h"
#include	"value.h"
#include	"opcode.h"
#else
#include	<nickle/nickle-config.h>
#include	<nickle/mem.h>
#include	<nickle/value.h>
#include	<nickle/opcode.h>
#endif
#include	<assert.h>

typedef struct _func	    *FuncPtr;
typedef struct _namespace   *NamespacePtr;
typedef struct _command	    *CommandPtr;

typedef struct _symbolBase {
    DataType	*data;
    SymbolPtr	next;
    Atom	name;
    Class	class;
    Type	*type;
    Bool	forward;    /* forward declaration of a function */
} SymbolBase;

typedef struct _symbolType {
    SymbolBase	symbol;
} SymbolType;

typedef struct _symbolGlobal {
    SymbolBase	symbol;
    BoxPtr	value;
} SymbolGlobal;

typedef struct _symbolLocal {
    SymbolBase	symbol;
    int		element;
    Bool	staticScope;	/* dynamic scope equal to static */
    CodePtr	code;
} SymbolLocal;

typedef struct _symbolNamespace {
    SymbolBase	    symbol;
    NamespacePtr    namespace;
} SymbolNamespace;

typedef struct _symbolException {
    SymbolBase	    symbol;
    Value	    doc;
} SymbolException;

typedef union _symbol {
    SymbolBase	    symbol;
    SymbolType	    type;
    SymbolGlobal    global;
    SymbolLocal	    local;
    SymbolNamespace namespace;
    SymbolException exception;
} Symbol;

extern SymbolPtr    NewSymbolType (Atom name, Type *type);
extern SymbolPtr    NewSymbolException (Atom name, Type *type, Value doc);
extern SymbolPtr    NewSymbolConst (Atom name, Type *type);
extern SymbolPtr    NewSymbolGlobal (Atom name, Type *type);
extern SymbolPtr    NewSymbolGlobalValue (Atom name, BoxPtr value);
extern SymbolPtr    NewSymbolArg (Atom name, Type *type);
extern SymbolPtr    NewSymbolStatic (Atom name, Type *Rep);
extern SymbolPtr    NewSymbolAuto (Atom name, Type *type);
extern SymbolPtr    NewSymbolNamespace (Atom name, NamespacePtr namespace);

static inline Type *_TypeNameType(TypePtr t) {
	if (t->name.name)
		return t->name.name->symbol.type;
	else
		return NULL;
}

#define TypeNameType(t) _TypeNameType(t)

typedef struct _namelist	*NamelistPtr;

typedef struct _namelist {
    DataType	    *data;
    NamelistPtr	    next;
    SymbolPtr	    symbol;
    Publish	    publish;
} Namelist;

typedef struct _namespace {
    DataType	    *data;
    NamespacePtr    previous;
    NamelistPtr	    names;
    Publish	    publish;
} Namespace;

NamespacePtr	NewNamespace (NamespacePtr previous);
SymbolPtr	NamespaceFindName (NamespacePtr namespace, Atom atom, Bool search);
Bool		NamespaceIsNamePrivate (NamespacePtr namespace, Atom atom, Bool search);
SymbolPtr	NamespaceAddName (NamespacePtr namespace, SymbolPtr symbol, Publish publish);
Bool		NamespaceRemoveName (NamespacePtr namespace, Atom atom);
void		NamespaceImport (NamespacePtr namespace, NamespacePtr import, Publish publish);
void		NamespaceInit (void);

extern NamespacePtr GlobalNamespace, TopNamespace, CurrentNamespace, DebugNamespace, LexNamespace;
extern FramePtr	    CurrentFrame;

typedef struct _command {
    DataType		*data;
    CommandPtr		previous;
    Atom		name;
    Value		func;
    Bool		names;
} Command;

CommandPtr  NewCommand (CommandPtr previous, Atom name, Value func, Bool names);
CommandPtr  CommandFind (CommandPtr command, Atom name);
CommandPtr  CommandRemove (CommandPtr command, Atom name);

extern CommandPtr   CurrentCommands;

typedef struct _DeclList    *DeclListPtr;
typedef struct _DeclList {
    DataType	*data;
    DeclListPtr	next;
    Atom	name;
    SymbolPtr	symbol;
    ExprPtr	init;
} DeclList;

extern DeclListPtr  NewDeclList (Atom name, ExprPtr init, DeclListPtr next);

typedef struct _MemList    *MemListPtr;
typedef struct _MemList {
    DataType	*data;
    MemListPtr	next;
    Type	*type;
    AtomListPtr	atoms;
} MemList;

extern MemListPtr  NewMemList (AtomListPtr atoms, Type *type, MemListPtr next);

typedef struct _Fulltype {
    Publish publish;
    Class   class;
    Type   *type;
} Fulltype;
    
typedef struct _funcDecl {
    Fulltype	type;
    DeclList	*decl;
} FuncDecl;

typedef struct _frame {
    DataType	    *data;
    FramePtr	    previous;
    FramePtr	    staticLink;
    Value	    function;
    BoxPtr	    frame;
    BoxPtr	    statics;
    InstPtr	    savePc;
    ObjPtr	    saveObj;
} Frame;

extern FramePtr	NewFrame (Value		function,
			  FramePtr	previous,
			  FramePtr	staticLink,
			  BoxTypesPtr	dynamics,
			  BoxPtr	statics);

typedef struct _catch {
    Continuation	    continuation;
    SymbolPtr	    exception;
} Catch;

CatchPtr    NewCatch (Value thread, SymbolPtr exception);

typedef struct _twixt {
    Continuation	    continuation;
    InstPtr	    leave;
    int		    depth;
} Twixt;

TwixtPtr
NewTwixt (ContinuationPtr continuation, InstPtr enter, InstPtr leave);

InstPtr	    TwixtJump (Value thread, TwixtPtr twixt, Bool enter);

static inline int TwixtDepth(Twixt *t) {
	if (t)
		return t->depth;
	else
		return 0;
}

TwixtPtr    TwixtNext (TwixtPtr twixt, TwixtPtr last);

# define	NOTHING	0
# define	CONT	1
# define	BRK	2
# define	RET	3

typedef struct _exprBase {
    DataType	    *data;
    int		    tag;
    Atom	    file;
    int		    line;
    NamespacePtr    namespace;
    Type	    *type;
    double_digit    ticks;
    double_digit    sub_ticks;
} ExprBase;

typedef struct _exprTree {
    ExprBase	expr;
    ExprPtr	left;
    ExprPtr	right;
} ExprTree;

typedef struct _exprConst {
    ExprBase	expr;
    Value	constant;
} ExprConst;

typedef struct _exprAtom {
    ExprBase	expr;
    Atom	atom;
    SymbolPtr	symbol;
    Bool	privateFound;	/* used to clarify error message for missing names */
} ExprAtom;

typedef struct _exprCode {
    ExprBase	expr;
    CodePtr	code;
} ExprCode;

typedef struct _exprDecls {
    ExprBase	expr;
    DeclListPtr	decl;
    Class	class;
    Type	*type;
    Publish	publish;
} ExprDecl;

typedef struct _exprType {
    ExprBase	expr;
    ExprPtr	left;
    Type	*type;
} ExprType;

typedef union _expr {
    ExprBase	base;
    ExprTree	tree;
    ExprConst	constant;
    ExprAtom	atom;
    ExprCode	code;
    ExprDecl	decl;
    ExprType	type;
} Expr;

Expr	*NewExprTree (int tag, Expr *left, Expr *right);
Expr	*NewExprComma (Expr *left, Expr *right);
Expr	*NewExprConst (int tag, Value val);
Expr	*NewExprAtom (Atom atom, SymbolPtr symbol, Bool privateFound);
Expr	*NewExprCode (CodePtr code, ExprPtr name);
Expr	*NewExprDecl (int tag, DeclListPtr decl, Class class, Type *type, Publish publish);
Expr	*NewExprType (int tag, Expr *left, Type *type);
Expr	*ExprRehang (Expr *expr, Expr *right);


typedef struct _codeBase {
    DataType	*data;
    Bool	builtin;
    Type	*type;
    int		argc;
    Bool	varargs;
    ArgType	*args;
    ExprPtr	name;
    CodePtr	previous;
    CodePtr	func;	    /* function context, usually self */
    Value	doc;	    /* documentation */
} CodeBase;

/*
 * Static initializers:
 *
 *  Two kinds:
 *	static
 *	global
 *
 * Static initializers are essentially parallel functions called when
 * the function is evaluated; the value resulting from this evaluation
 * contains a pointer to the function body along with a block of initialized
 * statics.   They can therefore access any *static* variables in the
 * function scope as well as any other variables in *dynamic* scope
 * at the time of function evaluation, which means they can access
 * any name they can see *except* for dynamic variables in the function
 * scope.
 *
 * Global initializers are run in a global context, they can only access
 * variables which have global lifetime, that means only global variables.
 * When found inside a function context, they are placed in the static
 * initializer block of the enclosing function at global scope.
 *
 *
 *	function foo() {
 *	    auto foo_a = 1;
 *	    static foo_s = 2;
 *	    global foo_g = 3;
 *
 *	    function bar() {
 *		auto	bar_a1 = 4;	    <- can touch any name in scope
 *		global	bar_g1 = foo_g;	    <- legal
 *		global	bar_g2 = foo_s;	    <- not legal
 *		static	bar_s1 = foo_g;	    <- legal
 *		static	bar_s2 = foo_s;	    <- legal
 *		static	bar_s3 = foo_a;	    <- legal
 *		static	bar_s4 = bar_a1;    <- not legal
 *	    }
 *	}
 */

typedef struct _funcBody {
    ObjPtr	obj;
    BoxTypesPtr	dynamics;
} FuncBody, *FuncBodyPtr;

typedef struct _funcCode {
    CodeBase	base;
    ExprPtr	code;

    FuncBody	body;
    FuncBody	staticInit;
    
    BoxTypesPtr	statics;
    Bool	inStaticInit;
    Bool	inGlobalInit;
} FuncCode, *FuncCodePtr;

typedef union _builtinFunc {
    Value   (*builtinN)(int, Value *);
    Value   (*builtin0)(void);
    Value   (*builtin1)(Value);
    Value   (*builtin2)(Value,Value);
    Value   (*builtin3)(Value,Value,Value);
    Value   (*builtin4)(Value,Value,Value,Value);
    Value   (*builtin5)(Value,Value,Value,Value,Value);
    Value   (*builtin6)(Value,Value,Value,Value,Value,Value);
    Value   (*builtin7)(Value,Value,Value,Value,Value,Value,Value);
    Value   (*builtin8)(Value,Value,Value,Value,Value,Value,Value,Value);
    Value   (*builtinNJ)(InstPtr *, int, Value *);
    Value   (*builtin0J)(InstPtr *);
    Value   (*builtin1J)(InstPtr *,Value);
    Value   (*builtin2J)(InstPtr *,Value,Value);
} BuiltinFunc;

typedef struct _builtinCode {
    CodeBase	base;
    Bool	needsNext;
    BuiltinFunc	b;
} BuiltinCode, *BuiltinCodePtr;

typedef union _code {
    CodeBase	base;
    FuncCode	func;
    BuiltinCode	builtin;
} Code;

CodePtr	NewFuncCode (Type *type, ArgType *args, ExprPtr code, Value doc);
CodePtr	NewBuiltinCode (Type *type, ArgType *args, int argc, 
			BuiltinFunc func, Bool needsNext, char *doc);
Value	NewFunc (CodePtr, FramePtr);

typedef struct _farJump {
    DataType	*data;
    int		inst;
    int		twixt;
    int		catch;
    int		frame;
} FarJump, *FarJumpPtr;

FarJumpPtr  NewFarJump (int inst, int twixt, int catch, int frame);
Value	    FarJumpContinuation (ContinuationPtr continuation, FarJumpPtr farJump);

#define InstPush    0x01

typedef struct _instBase {
    OpCode	opCode;
    short	flags;
} InstBase;

typedef struct _instBox {
    InstBase	inst;
    BoxPtr	box;
} InstBox;

typedef struct _instFrame {
    InstBase	inst;
    short	staticLink;
    short	element;
} InstFrame;

typedef struct _instConst {
    InstBase	inst;
    Value	constant;
} InstConst;

typedef struct _instAtom {
    InstBase	inst;
    Atom	atom;
} InstAtom;

typedef struct _instInt {
    InstBase	inst;
    int		value;
} InstInt;

typedef struct _instStruct {
    InstBase	inst;
    StructType	*structs;
} InstStruct;

typedef struct _instArray {
    InstBase	inst;
    short    	ndim;
    short	resizable;
    TypePtr	type;
} InstArray;

typedef enum _aInitMode {
    AInitModeStart,	/* Build initialization data on stack */
    AInitModeElement,	/* Initialize one element */
    AInitModeRepeat,	/* Duplicate initialization values along row */
    AInitModeFunc,	/* Build stack frame for function call */
    AInitModeTest,	/* Check to see if the array is completely initialized */
    AInitModeFuncDone	/* Clean up */
} AInitMode;

typedef struct _instAInit {
    InstBase	inst;
    int		dim;
    AInitMode	mode;
} InstAInit;

typedef struct _instHash {
    InstBase	inst;
    TypePtr	type;
} InstHash;

typedef struct _instCode {
    InstBase	inst;
    CodePtr	code;
} InstCode;

typedef enum _branchMod {
    BranchModNone, BranchModBreak, BranchModContinue, 
    BranchModReturn, BranchModReturnVoid, BranchModCatch
} BranchMod;

typedef struct _instBranch {
    InstBase	inst;
    int		offset;
    BranchMod	mod;
} InstBranch;

typedef struct _instBinOp {
    InstBase	inst;
    BinaryOp	op;
} InstBinOp;

typedef struct _instBinFunc {
    InstBase	inst;
    BinaryFunc	func;
} InstBinFunc;

typedef struct _instUnOp {
    InstBase	inst;
    UnaryOp	op;
} InstUnOp;

typedef struct _instUnFunc {
    InstBase	inst;
    UnaryFunc	func;
} InstUnFunc;

typedef struct _instAssign {
    InstBase	inst;
    Bool	initialize;
} InstAssign;

typedef struct _instObj {
    InstBase	inst;
    ObjPtr	obj;
} InstObj;

typedef struct _instCatch {
    InstBase	inst;
    int		offset;
    SymbolPtr	exception;
} InstCatch;

typedef struct _instRaise {
    InstBase	inst;
    int		argc;
    SymbolPtr	exception;
} InstRaise;

typedef struct _instTwixt {
    InstBase	inst;
    int		enter;
    int		leave;
} InstTwixt;

typedef struct _instTagCase {
    InstBase	inst;
    int		offset;
    Atom	tag;
} InstTagCase;

typedef struct _instUnwind {
    InstBase	inst;
    int		twixt;
    int		catch;
} InstUnwind;

typedef struct _instFarJump {
    InstBase	inst;
    FarJumpPtr	farJump;
    BranchMod	mod;
} InstFarJump;

typedef struct _instIsType {
    InstBase	inst;
    TypePtr	type;
} InstIsType;

typedef union _inst {
    InstBase	base;
    InstBox	box;
    InstFrame	frame;
    InstConst	constant;
    InstAtom	atom;
    InstInt	ints;
    InstStruct	structs;
    InstArray	array;
    InstAInit	ainit;
    InstHash	hash;
    InstCode	code;
    InstBranch	branch;
    InstBinOp	binop;
    InstBinFunc	binfunc;
    InstUnOp	unop;
    InstUnFunc	unfunc;
    InstAssign	assign;
    InstObj	obj;
    InstCatch	catch;
    InstRaise	raise;
    InstTwixt	twixt;
    InstTagCase	tagcase;
    InstUnwind	unwind;
    InstFarJump farJump;
    InstIsType	isType;
} Inst;

/*
 * A structure used to process non-structured
 * control flow changes (break, continue, return)
 * within the presence of twixt/catch
 */

typedef enum _nonLocalKind { 
    NonLocalControl, NonLocalTwixt, NonLocalTry, NonLocalCatch
} NonLocalKind;

#define NON_LOCAL_RETURN    0
#define NON_LOCAL_BREAK	    1
#define NON_LOCAL_CONTINUE  2

typedef struct _nonLocal {
    DataType		*data;
    struct _nonLocal	*prev;
    NonLocalKind	kind;	/* kind of non local object */
    int			target;	/* what kind of targets */
    ObjPtr		obj;
} NonLocal, *NonLocalPtr;

typedef struct _Stat {
    int		inst;
    ExprPtr	stat;
} Stat, *StatPtr;

typedef struct _obj {
    DataType	*data;
    int		size;
    int		used;
    int		size_stat;
    int		used_stat;
    double_digit    ticks;
    double_digit    sub_ticks;
    Bool	error;
    NonLocal	*nonLocal;
    Inst	insts[0];
} Obj;

static inline InstPtr ObjCode(Obj *obj, int i) {
    return &(obj->insts[i]);
}

static inline int ObjLast(Obj *obj) {
    return obj->used - 1;
}

static inline StatPtr ObjStat(Obj *obj, int i) {
    StatPtr	stats = (StatPtr) ObjCode(obj,obj->size);
    return &stats[i];
}

ObjPtr	CompileStat (ExprPtr expr, CodePtr code);
ObjPtr	CompileExpr (ExprPtr expr, CodePtr code);
ExprPtr	ObjStatement (ObjPtr obj, InstPtr inst);

typedef enum _wakeKind {
    WakeAll, WakeOne
} WakeKind;

extern Bool lastThreadError;

Value	    NewThread (FramePtr frame, ObjPtr code);
void	    ThreadSleep (Value thread, Value sleep, int priority);
void	    ThreadStepped (Value thread);
void	    ThreadsWakeup (Value sleep, WakeKind wake);
void	    ThreadsRun (Value thread, Value lex);
void	    ThreadsInterrupt (void);
void	    ThreadsSignal (Value signal);
void	    ThreadsContinue (void);
void	    ThreadFinish (Value thread, Bool error);
void	    ThreadSetState (Value thread, ThreadState state);
void	    ThreadInit (void);
void	    TraceFunction (Value file, FramePtr frame, CodePtr code, ExprPtr name);
void	    TraceFrame (Value file, FramePtr frame, ObjPtr obj, InstPtr pc, int depth);
void	    ThreadStackDump (Value thread);
void	    ThreadsBlock (void);

typedef void	(*NickleBlockHandler) (void *closure);

void
ThreadsRegisterBlockHandler (NickleBlockHandler handler, void *closure);

void
ThreadsUnregisterBlockHandler (NickleBlockHandler handler, void *closure);

typedef struct _jump {
    DataType	    *data;
    TwixtPtr	    enter;
    TwixtPtr	    entering;
    TwixtPtr	    leave;
    TwixtPtr	    parent;
    ContinuationPtr	    continuation;
    Value	    ret;
} Jump;

Value	    JumpContinue (Value thread, InstPtr *next);
InstPtr	    JumpStart (Value thread, ContinuationPtr continuation, Value ret);
JumpPtr	    NewJump (TwixtPtr leave, TwixtPtr enter, 
		     TwixtPtr parent, ContinuationPtr continuation, Value ret);

extern Value	running;    /* current thread */
extern Value	stopped;    /* stopped threads */
extern Bool	complete;   /* must complete current inst */
extern Bool	profiling;  /* profiling is active */

void	    InstDump (InstPtr inst, int indent, int i, int *branch, int maxbranch);
void	    ObjDump (ObjPtr obj, int indent);

void	SymbolInit (void);

extern NamespacePtr    DebugNamespace;
extern NamespacePtr    FileNamespace;
extern NamespacePtr    MathNamespace;
#ifdef BSD_RANDOM
extern NamespacePtr    BSDRandomNamespace;
#endif
extern NamespacePtr    SemaphoreNamespace;
extern NamespacePtr    StringNamespace;
extern NamespacePtr    ThreadNamespace;
extern NamespacePtr	CommandNamespace;
#ifdef GCD_DEBUG
extern NamespacePtr	GcdNamespace;
#endif
extern NamespacePtr	EnvironNamespace;
extern NamespacePtr	DateNamespace;

void	BuiltinInit (void);

Bool	DebugSetFrame (Value continuation, int offset);
void	DebugStart (Value continuation);
void	DebugBreak (Value thread);

void	ProfileInterrupt (Value thread);

typedef Bool	(*TimerFunc) (void *closure);

unsigned long	TimeInMs (void);

void	TimerInsert (void *closure, TimerFunc func, 
		     unsigned long delta, unsigned long incr);
void	TimerInterrupt (void);
void	TimerInit (void);
		     
void	IoInit (void);
void	IoStart (void);
void	IoStop (void);
void	IoFini (void);
Bool	IoTimeout (void *);
void	IoNoticeWriteBlocked (void);
#ifdef NO_PIPE_SIGIO
void	IoNoticeReadBlocked (void);
#endif
void	IoNoticeTtyUnowned (void);
void	IoInterrupt (void);

void	FileFini (void);
void	ProcessInterrupt (void);

void	*AllocateTemp (int size);

typedef struct _ProfileData {
    double_digit    sub;
    double_digit    self;
} ProfileData;

void	PrettyPrint (Value f, Publish publish, SymbolPtr name);
void	PrettyCode (Value f, CodePtr code, Atom name, Class class, 
		    Publish publish, int level, Bool nest);
void	PrettyStat (Value F, Expr *e, Bool nest);
void	PrettyExpr (Value f, Expr *e, int parentPrec, int level, Bool nest);

void	EditFunction (SymbolPtr name, Publish publish);
void	EditFile (Value file_name);

Value	lookupVar (char *ns, char *n);
void	setVar (NamespacePtr, char *, Value, Type *type);
void	GetNamespace (NamespacePtr *, FramePtr *, CodePtr *);
Bool	NamespaceLocate (Value names, NamespacePtr  *s, SymbolPtr *symbol, Publish *publish, Bool complain);
ExprPtr	BuildName (char *ns_name, char *name);
ExprPtr	BuildCall (char *, char *, int, ...);
ExprPtr	BuildFullname (ExprPtr colonnames, Atom name);
ExprPtr	BuildRawname (ExprPtr colonnames, Atom name);

Atom	LexFileName (void);
int	LexFileLine (void);
Bool	LexInteractive (void);
Bool	LexResetInteractive (void);
void	LexInit (void);
void	NewLexInput (Value file, Atom name, Bool after, Bool interactive);
#if HAVE_LIBREADLINE
extern volatile int stdin_in_readline;
#endif

int	yywrap (void);
void	yyerror (char *msg);
void	ParseError (char *fmt, ...);
int	yylex (void);
Bool	LexFile (char *file, Bool complain, Bool after);
Value	atov (char *, int), aetov (char *, int);
extern int  ignorenl;
void	skipcomment (void);
Value	lexdoc (void);
void	skipline (void);
int	lexEscape (int);

extern void	init (void);
extern int	yyparse (void);
extern int	stdin_interactive;

void	intr(int);
void	stop (int), die (int), segv (int);

void
catchSignal (int sig, void (*func) (int sig));

void
resetSignal (int sig, void (*func) (int sig));

extern Value    yyinput;

/* Standard exceptions */
typedef enum _standardException {
    exception_none,
    exception_uninitialized_value,  /* */
    exception_invalid_argument,	    /* string integer poly */
    exception_readonly_box,	    /* poly */
    exception_invalid_array_bounds, /* poly poly */
    exception_divide_by_zero,	    /* number number */
    exception_invalid_struct_member,/* poly string */
    exception_invalid_binop_values, /* poly poly */
    exception_invalid_unop_value,   /* poly */
    exception_open_error,	    /* string integer string */
    exception_io_error,		    /* string integer file */
    exception_name_error,	    /* string integer string */
    exception_signal,		    /* integer */
    exception_system_error,	    /* string integer poly */
    exception_io_eof,		    /* file */
    _num_standard_exceptions
} StandardException;

void
RaiseException (Value thread, SymbolPtr exception, Value ret, InstPtr *next);

void
RegisterStandardException (StandardException	se,
			   SymbolPtr		sym);

void
RaiseStandardException (StandardException   se,
			int		    argc,
			...);

SymbolPtr
CheckStandardException (void);

Value
JumpStandardException (Value thread, InstPtr *next);

static inline Value
IntIncDec (Value av, BinaryOp operator)
{
    int	a = ValueInt(av);

    if (operator == PlusOp)
	a++;
    else
	a--;
    if (NICKLE_INT_CARRIED(a))
	return NewIntInteger (a);
    return NewInt (a);
}

static inline Value ValueIncDec(Value v, BinaryOp o) {
	if (ValueIsInt(v))
		return IntIncDec(v,o);
	else
		return BinaryOperate(v, One, o);
}

static inline Value BoxValue (BoxPtr box, int e) {
    Value   v = BoxElements(box)[e];
    if (!v)
    {
	RaiseStandardException (exception_uninitialized_value, 0);
	return (Void);
    }
    return v;
}

static inline Value Dereference (Value v) {
    if (!ValueIsRef(v)) {
	RaiseStandardException (exception_invalid_unop_value, 1,
				v);
	return Void;
    }
    v = RefValueGet(v);
    if (!v) {
	RaiseStandardException (exception_uninitialized_value, 0);
	return (Void);
    }
    return REFERENCE (v);
}

/* vararg builtins */
Value	do_printf (int, Value *);
Value	do_fprintf (int, Value *);
Value	do_imprecise (int, Value *);
Value	do_Thread_kill (int, Value *);
Value	do_Thread_trace (int, Value *);
Value	do_Thread_trace (int, Value *);
Value	do_History_show (int, Value *);
Value	do_string_to_integer (int, Value *);
Value	do_Semaphore_new (int, Value *);
Value	do_Command_undefine (int, Value *);
Value	do_Command_pretty_print (int , Value *);

/* zero argument builtins */
Value	do_Thread_cont (void);
Value	do_Thread_current (void);
Value	do_Thread_list (void);
Value	do_time (void);
Value	do_File_string_write (void);
Value	do_Debug_up (void);
Value	do_Debug_down (void);
Value	do_Debug_done (void);
Value	do_Debug_collect (void);
Value   do_Debug_help (void);
Value	do_File_mkpipe (void);
Value	do_millis (void);

/* one argument builtins */
Value	do_sleep (Value);
Value	do_exit (Value);
Value	do_dim (Value);
Value	do_dims (Value);
Value	do_reference (Value);
Value	do_string_to_real (Value);
Value	do_abs (Value);
Value	do_floor (Value);
Value	do_func_args (Value);
Value	do_ceil (Value);
Value	do_exponent (Value);
Value	do_mantissa (Value);
Value	do_numerator (Value);
Value	do_denominator (Value);
Value	do_precision (Value);
Value	do_sign (Value);
Value	do_bit_width (Value);
Value	do_is_int (Value);
Value	do_is_rational (Value);
Value	do_is_number (Value);
Value	do_is_string (Value);
Value	do_is_file (Value);
Value	do_is_thread (Value);
Value	do_is_semaphore (Value);
Value	do_is_continuation (Value);
Value	do_is_array (Value);
Value	do_is_hash (Value);
Value	do_is_ref (Value);
Value	do_is_struct (Value);
Value	do_is_func (Value);
Value	do_is_bool (Value);
Value	do_is_void (Value);
Value	do_is_uninit (Value);
Value	do_make_uninit (Value);
Value	do_hash (Value);
Value	do_Thread_get_priority (Value);
Value	do_Thread_id_to_thread (Value);
Value	do_Thread_join (Value);
Value	do_Semaphore_signal (Value);
Value	do_Semaphore_count (Value);
Value	do_Semaphore_wait (Value);
Value	do_Semaphore_test (Value);
Value	do_File_close (Value);
Value	do_File_flush (Value);
Value	do_File_getb (Value);
Value	do_File_getc (Value);
Value	do_File_end (Value);
Value	do_File_error (Value);
Value	do_File_clear_error (Value);
Value	do_File_string_read (Value);
Value	do_File_string_string (Value);
Value	do_File_isatty (Value);
Value	do_File_status (Value);
Value	do_File_unlink (Value);
Value	do_File_rmdir (Value);
Value	do_String_length (Value);
Value	do_String_new (Value);
Value	do_Primitive_random (Value);
Value	do_Primitive_srandom (Value);
Value	do_Debug_dump (Value);
Value	do_Debug_dump_active (void);
Value	do_Command_delete (Value);
Value	do_Command_edit (Value);
Value	do_Command_display (Value);
Value	do_Command_valid_name (Value);
Value	do_Environ_check (Value);
Value	do_Environ_get (Value);
Value	do_Environ_unset (Value);
Value	do_profile (Value);

/* two argument builtins */
Value	do_Thread_set_priority (Value, Value);
Value	do_Thread_signal (Value, Value);
Value	do_File_open (Value, Value);
Value	do_gcd (Value, Value);
Value	do_xor (Value, Value);
Value	do_Math_pow (Value, Value);
Value	do_Math_assignpow (Value, Value);
Value	do_Math_popcount (Value);
Value   do_Math_factorial (Value);
Value	do_File_putb (Value, Value);
Value	do_File_putc (Value, Value);
Value	do_File_ungetb (Value, Value);
Value	do_File_ungetc (Value, Value);
Value	do_File_setbuf (Value, Value);
Value	do_File_rename (Value, Value);
Value	do_File_mkdir (Value, Value);
Value	do_String_index (Value, Value);
Value	do_setjmp (Value, Value);
Value	do_setdims (Value, Value);
Value	do_setdim (Value, Value);
Value	do_Command_new (Value, Value);
Value	do_Command_new_names (Value, Value);
#ifdef GCD_DEBUG
Value	do_Gcd_bdivmod (Value, Value);
Value	do_Gcd_kary_reduction (Value, Value);
#endif
Value	do_Environ_set (Value, Value);

/* three argument builtins */
Value	do_File_vfprintf (Value, Value, Value);
Value	do_String_substr (Value, Value, Value);
Value	do_File_reopen (Value, Value, Value);
Value	do_File_filter (Value file, Value argv, Value filev);

/* four argument builtins */
Value	do_Command_lex_input (Value file, Value name, Value after, Value interactive);

/* seven argument builtins */
Value	do_File_print (Value, Value, Value, Value, Value, Value, Value);

/* two argument non-local builtins */
Value	do_longjmp (InstPtr *, Value, Value);

/* hash builtins (for testing) */
Value	do_hash_new (void);
Value	do_hash_get (Value, Value);
Value	do_hash_del (Value, Value);
Value	do_hash_test (Value, Value);
Value	do_hash_set (Value, Value, Value);
Value	do_hash_keys (Value);
