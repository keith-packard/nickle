/* $Header$ */
/*
 * This program is Copyright (C) 1988 by Keith Packard.  NICK is provided to
 * you without charge, and with no warranty.  You may give away copies of
 * NICK, including source, provided that this notice is included in all the
 * files.
 */
/*
 *	nick.h
 *
 */

#include "value.h"
#include "opcode.h"

typedef union _expr	*ExprPtr;
#if 0
typedef union _type	*TypePtr;
#endif
typedef union _symbol	*SymbolPtr;
typedef struct _func	*FuncPtr;
typedef struct _scope	*ScopePtr;

#if 0
typedef struct _typeBase {
    DataType	*data;
    int		tag;
} TypeBase;

typedef struct _typeRef {
    TypeBase	type;
    TypePtr	ref;
} TypeTree;

typedef struct _typeName {
    TypeBase	type;
    Atom	name;
} TypeName;

typedef struct _typeExpr {
    TypeBase	type;
    ExprPtr	expr;
} TypeExpr;

typedef union _type {
    TypeBase	base;
    TypeTree	tree;
    TypeName	name;
    TypeExpr	expr;
} Type;
#endif

typedef struct _symbolBase {
    DataType	*data;
    SymbolPtr	next;
    Atom	name;
    Class	class;
    Type	type;
    Publish	publish;
} SymbolBase;

#if 0
typedef struct _symbolType {
    SymbolBase	symbol;
} SymbolType;
#endif

typedef struct _symbolGlobal {
    SymbolBase	symbol;
    BoxPtr	value;
} SymbolGlobal;

typedef struct _symbolLocal {
    SymbolBase	symbol;
    int		element;
} SymbolLocal;

typedef struct _symbolStruct {
    SymbolBase	symbol;
    StructType	*type;
} SymbolStruct;

typedef struct _symbolScope {
    SymbolBase	symbol;
    ScopePtr	scope;
} SymbolScope;

typedef union _symbol {
    SymbolBase	    symbol;
#if 0
    SymbolType	    type;
#endif
    SymbolGlobal    global;
    SymbolLocal	    local;
    SymbolStruct    structs;
    SymbolScope	    scope;
} Symbol;

#if 0
extern SymbolPtr    NewSymbolType (Atom name, Type type);
#endif
extern SymbolPtr    NewSymbolGlobal (Atom name, Type type, Publish publish);
extern SymbolPtr    NewSymbolArg (Atom name, Type type);
extern SymbolPtr    NewSymbolStatic (Atom name, Type type);
extern SymbolPtr    NewSymbolAuto (Atom name, Type type);
extern SymbolPtr    NewSymbolStruct (Atom name, StructType *type, Publish publish);
extern SymbolPtr    NewSymbolScope (Atom name, ScopePtr scope, Publish publish);

typedef struct _scopeChain {
    DataType	*data;
    struct _scopeChain	*next;
    SymbolPtr	symbol;
    Publish	publish;
} ScopeChain, *ScopeChainPtr;

typedef struct _scope {
    DataType	*data;
    ScopePtr	previous;
    ScopeChainPtr   symbols;
    CodePtr	code;
    Publish	publish;
} Scope;

extern ScopePtr	    NewScope (ScopePtr previous);
extern SymbolPtr    ScopeLookupSymbol (ScopePtr scope, Atom name);
extern SymbolPtr    ScopeFindSymbol (ScopePtr scope, Atom name, int *depth);
extern SymbolPtr    ScopeAddSymbol (ScopePtr scope, SymbolPtr symbol);
extern Bool	    ScopeRemoveSymbol (ScopePtr scope, SymbolPtr symbol);
extern SymbolPtr    ScopeImport (ScopePtr scope, ScopePtr import, Publish publish);
extern ScopePtr	    GlobalScope;
extern void	    ScopeInit (void);

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
    Type	type;
    DeclListPtr	names;
} MemList;

extern MemListPtr  NewMemList (DeclListPtr names, Type type, MemListPtr next);

typedef struct _Fulltype {
    Class   class;
    Type    type;
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

# define	NOTHING	0
# define	CONT	1
# define	BRK	2
# define	RET	3

typedef struct _exprBase {
    DataType	*data;
    int		tag;
    ScopePtr	scope;
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
    Type	type;
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
Expr	*NewExprDecl (DeclListPtr decl, Class class, Type type, Publish publish);

typedef struct _codeBase {
    DataType	*data;
    Bool	builtin;
    Type	type;
    int		argc;
    Atom	name;
} CodeBase;

typedef struct _funcCode {
    CodeBase	base;
    ExprPtr	args;
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
} BuiltinFunc;

typedef struct _builtinCode {
    CodeBase	base;
    BuiltinFunc	b;
} BuiltinCode, *BuiltinCodePtr;

typedef union _code {
    CodeBase	base;
    FuncCode	func;
    BuiltinCode	builtin;
} Code;

CodePtr	NewFuncCode (Type, ExprPtr args, ExprPtr code);
CodePtr	NewBuiltinCode (Type, int, BuiltinFunc func);
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

typedef union _inst {
    InstBase	base;
    InstVar	var;
    InstConst	constant;
    InstAtom	atom;
    InstInt	ints;
    InstCode	code;
    InstBranch	branch;
    InstObj	obj;
} Inst;

typedef struct _obj {
    DataType	*data;
    int		size;
    int		used;
    int		errors;
} Obj;

#define ObjCode(obj,i)	(((InstPtr) (obj + 1)) + (i))
#define ObjLast(obj)	((obj)->used - 1)

ObjPtr	CompileStat (ExprPtr expr, ScopePtr scope);
ObjPtr	CompileExpr (ExprPtr expr, ScopePtr scope);

Value	    NewThread (FramePtr frame, ObjPtr code);
ThreadState ThreadStep (Value thread);
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
Value	    ThreadFromId (Value);
Value	    ThreadJoin (Value);

#define Runnable(t)    (((t)->thread.state & ~(ThreadSuspended)) == 0)

extern Value	running;    /* current thread */
extern Value	stopped;    /* stopped threads */
extern Bool	complete;   /* must complete current inst */
extern int	runnable;   /* number of non-broken threads */

Value	    NewMutex (void);
Value	    MutexAcquire (Value), MutexTryAcquire (Value), MutexRelease (Value);

Value	    NewSemaphore (void);
Value	    SemaphoreWait (Value), SemaphoreTest (Value), SemaphoreSignal (Value);

void	    ObjDump (ObjPtr obj);

Symbol	*checkSym(Symbol *, Class, Type);
Symbol	*extractSym (Symbol *);
Symbol	*insertSym (char *);
void	SymbolInit (void);

void	BuiltinInit (void);

Value	History (Value, Value);
void	HistorySet (Value, Value);
void	HistoryDisplay (Value, Value *, Value *);
void	HistoryInsert (Value);
void	HistoryInit (void);

typedef Bool	(*TimerFunc) (void *closure);

unsigned long	TimeInMs (void);

void	TimerInsert (void *closure, TimerFunc func, 
		     unsigned long delta, unsigned long incr);
void	TimerInterrupt (void);
Value	Sleep (Value);
void	TimerInit (void);
		     
void	IoInit (void);
void	IoStart (void);
void	IoStop (void);
void	IoFini (void);
Bool	IoTimeout (void *);
void	IoNoticeWriteBlocked (void);
void	IoNoticeTtyUnowned (void);
void	IoInterrupt (void);

void	*AllocateTemp (int size);

void	PrettyPrint (Value f, SymbolPtr name);
void	PrintCode (Value f, CodePtr code, char *name, Publish publish, int level, Bool nest);
void	PrintStat (Value F, Expr *e, Bool nest);
void	EditFunction (SymbolPtr name);
void	EditFile (Value file_name);

void	print (Value f, Value value);
Value	lookupVar (char *);
void	setVar (char *, Value);
void	GetScope (ScopePtr *, FramePtr *);
ExprPtr	BuildName (char *);
ExprPtr	BuildCall (char *, int, ...);
/* int	yywrap (void); */

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
Value	doprintf (int, Value *);

extern void	init (void);
extern int	yyparse (void);
extern int	interactive, stdin_interactive;

void	intr(int);
void	stop (int), die (int), segv (int);
void	ignore_ferr (void);

extern Value    yyinput;
