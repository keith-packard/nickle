/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"value.h"
#include	"opcode.h"

typedef union _symbol	    *SymbolPtr;
typedef struct _func	    *FuncPtr;
typedef struct _namespace   *NamespacePtr;
typedef struct _typespace   *TypespacePtr;

typedef struct _symbolBase {
    DataType	*data;
    SymbolPtr	next;
    Atom	name;
    Class	class;
    Types	*type;
    Publish	publish;
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
} SymbolLocal;

typedef struct _symbolNamespace {
    SymbolBase	    symbol;
    NamespacePtr    namespace;
} SymbolNamespace;

typedef struct _symbolException {
    SymbolBase	    symbol;
} SymbolException;

typedef union _symbol {
    SymbolBase	    symbol;
    SymbolType	    type;
    SymbolGlobal    global;
    SymbolLocal	    local;
    SymbolNamespace namespace;
    SymbolException exception;
} Symbol;

extern SymbolPtr    NewSymbolType (Atom name, Types *type, Publish publish);
extern SymbolPtr    NewSymbolException (Atom name, Types *type, Publish publish);
extern SymbolPtr    NewSymbolGlobal (Atom name, Types *type, Publish publish);
extern SymbolPtr    NewSymbolArg (Atom name, Types *type);
extern SymbolPtr    NewSymbolStatic (Atom name, Types *Type);
extern SymbolPtr    NewSymbolAuto (Atom name, Types *type);
extern SymbolPtr    NewSymbolNamespace (Atom name, Publish publish);

typedef struct _namespaceChain {
    DataType		    *data;
    struct _namespaceChain  *next;
    SymbolPtr		    symbol;
    Publish		    publish;
} NamespaceChain, *NamespaceChainPtr;

typedef struct _namespace {
    DataType		*data;
    NamespacePtr	previous;
    NamespaceChainPtr   symbols;
    CodePtr		code;
    Publish		publish;
    Bool		debugger;
} Namespace;

extern NamespacePtr NewNamespace (NamespacePtr previous);
extern SymbolPtr    NamespaceLookupSymbol (NamespacePtr namespace, Atom name);
extern SymbolPtr    NamespaceFindSymbol (NamespacePtr namespace, Atom name, int *depth);
extern SymbolPtr    NamespaceAddSymbol (NamespacePtr namespace, SymbolPtr symbol);
extern Bool	    NamespaceRemoveSymbol (NamespacePtr namespace, SymbolPtr symbol);
extern SymbolPtr    NamespaceImport (NamespacePtr namespace, NamespacePtr import, Publish publish);
extern Class	    NamespaceDefaultClass (NamespacePtr namespace);
extern NamespacePtr GlobalNamespace, CurrentNamespace, DebugNamespace;
extern FramePtr	    CurrentFrame;
extern void	    NamespaceInit (void);

typedef struct _typespace {
    DataType		*data;
    TypespacePtr	previous;
    Atom		name;
    Bool		mask;
} Typespace;

extern TypespacePtr NewTypespace (TypespacePtr previous, Atom name, Bool mask);
extern TypespacePtr TypespaceFind (TypespacePtr typespace, Atom name);
extern TypespacePtr TypespaceRemove (TypespacePtr typespace, Atom name);

extern TypespacePtr CurrentTypespace;

typedef struct _DeclList    *DeclListPtr;
typedef struct _DeclList {
    DataType	*data;
    DeclListPtr	next;
    Atom	name;
    ExprPtr	init;
} DeclList;

extern DeclListPtr  NewDeclList (Atom name, ExprPtr init, DeclListPtr next);

typedef struct _MemList    *MemListPtr;
typedef struct _MemList {
    DataType	*data;
    MemListPtr	next;
    Types	*type;
    DeclListPtr	names;
} MemList;

extern MemListPtr  NewMemList (DeclListPtr names, Types *type, MemListPtr next);

typedef struct _Fulltype {
    Class   class;
    Types   *type;
    Publish publish;
} Fulltype;
    
typedef struct _frame {
    DataType	    *data;
    FramePtr	    previous;
    FramePtr	    staticLink;
    Value	    function;
    BoxPtr	    frame;
    BoxPtr	    statics;
    InstPtr	    savePc;
    ObjPtr	    saveCode;
} Frame;

extern FramePtr	NewFrame (Value		function,
			  FramePtr	previous,
			  FramePtr	staticLink,
			  BoxTypesPtr	dynamics,
			  BoxPtr	statics);

typedef struct _catch {
    DataType	    *data;
    CatchPtr	    previous;
    Value	    continuation;
    SymbolPtr	    exception;
} Catch;

CatchPtr    NewCatch (CatchPtr previous, Value continuation, SymbolPtr exception);

typedef struct _twixt {
    DataType	    *data;
    TwixtPtr	    previous;
    int		    depth;
    FramePtr	    frame;
    InstPtr	    enter;
    InstPtr	    leave;
    CatchPtr	    catches;
    StackObject	    *stack;
} Twixt;

TwixtPtr
NewTwixt (TwixtPtr	previous,
	  FramePtr	frame, 
	  InstPtr	enter,
	  InstPtr	leave,
	  CatchPtr	catches,
	  StackObject	*stack);

void	    TwixtJump (Value thread, TwixtPtr twixt, Bool enter, InstPtr *next);
int	    TwixtDepth (TwixtPtr twixt);
TwixtPtr    TwixtNext (TwixtPtr twixt, TwixtPtr last);

# define	NOTHING	0
# define	CONT	1
# define	BRK	2
# define	RET	3

typedef struct _exprBase {
    DataType	    *data;
    int		    tag;
    NamespacePtr    namespace;
    Types	    *type;
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
} ExprAtom;

typedef struct _exprCode {
    ExprBase	expr;
    CodePtr	code;
} ExprCode;

typedef struct _exprDecls {
    ExprBase	expr;
    DeclListPtr	decl;
    Class	class;
    Types	*type;
    Publish	publish;
} ExprDecl;

typedef union _expr {
    ExprBase	base;
    ExprTree	tree;
    ExprConst	constant;
    ExprAtom	atom;
    ExprCode	code;
    ExprDecl	decl;
} Expr;

Expr	*NewExprTree (int tag, Expr *left, Expr *right);
Expr	*NewExprConst (Value val);
Expr	*NewExprAtom (Atom atom);
Expr	*NewExprCode (CodePtr code, Atom name);
Expr	*NewExprDecl (DeclListPtr decl, Class class, Types *type, Publish publish);

typedef struct _codeBase {
    DataType	*data;
    Bool	builtin;
    Types	*type;
    int		argc;
    ArgType	*args;
    Atom	name;
} CodeBase;

typedef struct _funcCode {
    CodeBase	base;
    ExprPtr	code;
    ObjPtr	obj;
    
    BoxTypesPtr	dynamics;
    BoxTypesPtr	statics;
    
    ObjPtr	staticInit;
    Bool	inStaticInit;
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

CodePtr	NewFuncCode (Types *type, ArgType *args, ExprPtr code);
CodePtr	NewBuiltinCode (Types *type, ArgType *args, int argc, 
			BuiltinFunc func, Bool needsNext);
Value	NewFunc (CodePtr, FramePtr);

typedef struct _instBase {
    OpCode	opCode;
    Bool	push;
    ExprPtr	stat;
} InstBase;

typedef struct _instVar {
    InstBase	inst;
    SymbolPtr	name;
    int		staticLink;
} InstVar;

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

typedef struct _instCode {
    InstBase	inst;
    CodePtr	code;
} InstCode;

typedef struct _instBranch {
    InstBase	inst;
    int		offset;
} InstBranch;

typedef struct _instObj {
    InstBase	inst;
    ObjPtr	obj;
} InstObj;

typedef struct _instCatch {
    InstBase	inst;
    SymbolPtr	exception;
    int		offset;
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

typedef union _inst {
    InstBase	base;
    InstVar	var;
    InstConst	constant;
    InstAtom	atom;
    InstInt	ints;
    InstStruct	structs;
    InstCode	code;
    InstBranch	branch;
    InstObj	obj;
    InstCatch	catch;
    InstRaise	raise;
    InstTwixt	twixt;
} Inst;

typedef struct _obj {
    DataType	*data;
    int		size;
    int		used;
    int		errors;
} Obj;

#define ObjCode(obj,i)	(((InstPtr) (obj + 1)) + (i))
#define ObjLast(obj)	((obj)->used - 1)

ObjPtr	CompileStat (ExprPtr expr, NamespacePtr namespace);
ObjPtr	CompileExpr (ExprPtr expr, NamespacePtr namespace);

Value	    NewThread (FramePtr frame, ObjPtr code);
void	    ThreadSleep (Value thread, Value sleep, int priority);
void	    ThreadStepped (Value thread);
void	    ThreadsWakeup (Value sleep);
void	    ThreadsRun (Value thread, Value lex);
void	    ThreadsInterrupt (void);
void	    ThreadsContinue (void);
void	    ThreadFinish (Value thread);
void	    ThreadSetState (Value thread, ThreadState state);
void	    ThreadClearState (Value thread, ThreadState state);
void	    ThreadInit (void);
void	    TraceFunction (FramePtr frame, CodePtr code, Atom name);
void	    TraceFrame (FramePtr frame, InstPtr pc);
#ifdef DEBUG_JUMP
void	    TraceContinuation (char	    *where,
			       FramePtr	    frame,
			       StackObject  *stack,
			       CatchPtr	    catches,
			       TwixtPtr	    twixts,
			       InstPtr	    pc,
			       int	    indent);
void	    ContinuationTrace (char	*where, Value continuation);
#endif
void	    ContinuationJump (Value thread, Value continuation, InstPtr *next);
void	    ContinuationArgs (Value thread, BoxPtr args);

typedef struct _jump {
    DataType	    *data;
    TwixtPtr	    enter;
    TwixtPtr	    entering;
    TwixtPtr	    leave;
    TwixtPtr	    parent;
    Value	    continuation;
    Value	    ret;
    BoxPtr	    args;
} Jump;

Value	    JumpContinuation (JumpPtr jump, InstPtr *next);
JumpPtr	    JumpBuild (TwixtPtr leave, TwixtPtr enter, 
		       Value continuation, Value ret, InstPtr *next);
JumpPtr	    NewJump (TwixtPtr leave, TwixtPtr enter, 
		     TwixtPtr parent, Value continuation, Value ret);

#define Runnable(t)    (((t)->thread.state & ~(ThreadSuspended)) == 0)

extern Value	running;    /* current thread */
extern Value	stopped;    /* stopped threads */
extern Bool	complete;   /* must complete current inst */
extern int	runnable;   /* number of non-broken threads */

void	    ObjDump (ObjPtr obj, int indent);

Symbol	*checkSym(Symbol *, Class, Type);
Symbol	*extractSym (Symbol *);
Symbol	*insertSym (char *);
void	SymbolInit (void);

void	BuiltinInit (void);

Bool	DebugSetFrame (Value thread, int offset);

Value	History (Value, Value);
void	HistoryDisplay (Value, Value *, Value *);
void	HistoryInsert (Value);
void	HistoryInit (void);

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
void	IoNoticeReadBlocked (void);
void	IoNoticeTtyUnowned (void);
void	IoInterrupt (void);

void	*AllocateTemp (int size);

void	PrettyPrint (Value f, SymbolPtr name);
void	PrintCode (Value f, CodePtr code, Atom name, Class class, Publish publish,
		   int level, Bool nest);
void	PrintStat (Value F, Expr *e, Bool nest);
void	EditFunction (SymbolPtr name);
void	EditFile (Value file_name);

void	print (Value f, Value value);
Value	lookupVar (char *);
void	setVar (char *, Value);
void	GetNamespace (NamespacePtr *, FramePtr *);
ExprPtr	BuildName (char *);
ExprPtr	BuildCall (char *, char *, int, ...);
void	fprintTypes (Value f, Types *t);

int	yywrap (void);
void	yyerror (char *fmt, ...);
int	yylex (void);
Bool	pushinput (char *, Bool);
void	yyprompt (void);
Value	atov (char *, int), aetov (char *);
extern int  ignorenl;
void	skipcomment (void);
void	skipline (void);
int	lexEscape (int);
int	popinput (void);
Bool	lexfile (char *);
void	lexstdin (void);

void	callformat (Value f, char *fmt, int n, Value *p);
int	dofformat (Value, char *, int, Value *);

extern void	init (void);
extern int	yyparse (void);
extern int	interactive, stdin_interactive;

void	intr(int);
void	stop (int), die (int), segv (int);

extern Value    yyinput;

/* vararg builtins */
Value	do_printf (int, Value *);
Value	do_scanf (int, Value *);
Value	do_fprintf (int, Value *);
Value	do_imprecise (int, Value *);
Value	do_Thread_kill (int, Value *);
Value	do_Thread_trace (int, Value *);
Value	do_Thread_trace (int, Value *);
Value	do_History_show (int, Value *);
Value	do_string_to_integer (int, Value *);

/* zero argument builtins */
Value	do_Thread_cont (void);
Value	do_Thread_current (void);
Value	do_Mutex_new (void);
Value	do_Semaphore_new (void);
Value	do_Thread_list (void);
Value	do_getchar (void);
Value	do_time (void);
Value	do_Debug_up (void);
Value	do_Debug_down (void);
Value	do_Debug_done (void);
Value	do_Debug_collect (void);

/* one argument builtins */
Value	do_putchar (Value);
Value	do_sleep (Value);
Value	do_dim (Value);
Value	do_dims (Value);
Value	do_string_to_real (Value);
Value	do_abs (Value);
Value	do_floor (Value);
Value	do_ceil (Value);
Value	do_exponent (Value);
Value	do_mantissa (Value);
Value	do_numerator (Value);
Value	do_denominator (Value);
Value	do_precision (Value);
Value	do_sign (Value);
Value	do_is_integer (Value);
Value	do_is_rational (Value);
Value	do_is_number (Value);
Value	do_is_string (Value);
Value	do_is_file (Value);
Value	do_is_thread (Value);
Value	do_is_mutex (Value);
Value	do_is_semaphore (Value);
Value	do_is_continuation (Value);
Value	do_is_array (Value);
Value	do_is_ref (Value);
Value	do_is_struct (Value);
Value	do_is_func (Value);
Value	do_Thread_get_priority (Value);
Value	do_Thread_id_to_thread (Value);
Value	do_Thread_join (Value);
Value	do_Mutex_acquire (Value);
Value	do_Mutex_release (Value);
Value	do_Mutex_try_acquire (Value);
Value	do_Semaphore_signal (Value);
Value	do_Semaphore_test (Value);
Value	do_Semaphore_wait (Value);
Value	do_History_insert (Value);
Value	do_File_close (Value);
Value	do_File_flush (Value);
Value	do_File_getc (Value);
Value	do_String_length (Value);
Value	do_Primitive_random (Value);
Value	do_Primitive_srandom (Value);
Value	do_Debug_dump (Value);

/* two argument builtins */
Value	do_Thread_set_priority (Value, Value);
Value	do_File_open (Value, Value);
Value	do_gcd (Value, Value);
Value	do_Math_pow (Value, Value);
Value	do_Math_assignpow (Value, Value);
Value	do_File_putc (Value, Value);
Value	do_File_setbuf (Value, Value);
Value	do_String_index (Value, Value);
Value	do_set_jump (Value, Value);

/* three argument builtins */
Value	do_String_substr (Value, Value, Value);

/* seven argument builtins */
Value	do_File_print (Value, Value, Value, Value, Value, Value, Value);

/* two argument non-local builtins */
Value	do_long_jump (InstPtr *, Value, Value);
