/* $Header$ */
/*
 * This program is Copyright (C) 1988 by Keith Packard.  NICK is provided to
 * you without charge, and with no warranty.  You may give away copies of
 * NICK, including source, provided that this notice is included in all the
 * files.
 */
/*
 *	grammar for nick
 */

%{

# include	<math.h>
# include	"nick.h"

int ignorenl;
extern int	yyfiledeep;

void yyerror (char *fmt, ...);

static void
DeclListMark (void *object)
{
    DeclListPtr	dl = object;

    MemReference (dl->next);
    MemReference (dl->init);
}

DataType DeclListType = { DeclListMark, 0 };

DeclListPtr
NewDeclList (Atom name, ExprPtr init, DeclListPtr next)
{
    ENTER ();
    DeclListPtr	dl;

    dl = ALLOCATE (&DeclListType, sizeof (DeclList));
    dl->name = name;
    dl->init = init;
    dl->next = next;
    RETURN (dl);
}

static void
MemListMark (void *object)
{
    MemListPtr	ml = object;

    MemReference (ml->next);
    MemReference (ml->names);
}

DataType MemListType = { MemListMark, 0 };

MemListPtr
NewMemList (DeclListPtr names, Type type, MemListPtr next)
{
    ENTER ();
    MemListPtr	ml;

    ml = ALLOCATE (&MemListType, sizeof (MemList));
    ml->type = type;
    ml->names = names;
    ml->next = next;
    RETURN (ml);
}

%}

%union {
    int		    ival;
    Value	    vval;
    Class	    cval;
    Type	    tval;
    Publish	    pval;
    ExprPtr	    eval;
    Atom	    aval;
    DeclListPtr	    dval;
    MemListPtr	    mval;
    Fulltype	    ftval;
    ScopePtr	    sval;
    CodePtr	    fval;
}

%token <vval>	CONST
%token <ival>	NL ALL DOWN UP
%token <ival>	QUIT READ SHELL EDIT DEFAULT UNDEFINE HISTORY PRINT
%token <ival>	WHILE IF ELSE FOR DO BREAK CONTINUE EXPR RETURNTOK SCOPE IMPORT
%token <ival>	TRY CATCH
%token <ival>	FORK
%token <ival>	OP CP OC CC COMMA SEMI DOLLAR
%token <ival>	VAR
%token <ival>	ASSIGNPLUS ASSIGNMINUS ASSIGNTIMES ASSIGNDIVIDE ASSIGNDIV ASSIGNMOD
%token <ival>	ASSIGNPOW ASSIGNLXOR ASSIGNLAND ASSIGNLOR
%token <aval>	NAME 
%token <cval>	AUTO GLOBAL STATIC
%token <pval>	PUBLIC
%token <tval>	INT NATURAL INTEGER RATIONAL DOUBLE ARRAY FUNC EXCEPTION
%token <tval>	STRING POLYMORPH REF STRUCT TYPE FUNCTION
%token <ival>	AINIT
%type <eval>	expr primary stat optexpr statlist arglist oarglist
%type <eval>	history sexpr array oainits ainits aelem sinit sinits
%type <eval>	catch catches
%type <dval>	o_names names uninit_names
%type <mval>	memlist
%type <ftval>	type_def
%type <cval>	class
%type <tval>	type o_type
%type <pval>	publish o_publish
%type <vval>	opt_const
%type <fval>	fbody
%type <eval>	var_def ofargs fargs farg

%type <eval>	op_init
%type <ival>	assignop

%nonassoc <ival>	POUND
%right <ival>	COMMAOP
%right <ival>	ASSIGN
%right <ival>	QUEST COLON
%left <ival>	OR
%left <ival>	AND
%left <ival>	LOR
%left <ival>	LXOR
%left <ival>	LAND
%left <ival>	EQ NE
%left <ival>	LT GT LE GE
%left <ival>	PLUS MINUS
%left <ival>	TIMES DIVIDE DIV MOD
%right <ival>	POW
%right <ival>	UMINUS BANG FACT LNOT INC DEC STAR AMPER THREAD
%left <ival>	OS CS DOT ARROW CALL
%%
lines	:	lines pcommand
	|
	;
pcommand:	command
	|	error { ignorenl = 0; } eend
	;
eend	:	NL { yyerrok; }
	|	SEMI {}
	;
command	:	QUIT NL
			{ YYACCEPT; }
	|	expr NL
			{
			    ENTER ();
			    ExprPtr	e;
			    Value	t;
			    ScopePtr	s;
			    FramePtr	f;
			    
			    e = NewExprTree (COMMA,
					     BuildCall ("printf",
							2,
							BuildName ("format"),
							BuildCall ("HistoryInsert",
								   1, $1)),
					     BuildCall ("putchar",
							1,
							NewExprConst (NewInt ('\n'))));
			    GetScope (&s, &f);
			    t = NewThread (f, CompileExpr (e, s));
			    ThreadsRun (t, 0);
			    EXIT ();
			}
	|	expr POUND expr NL
			{
			    ENTER ();
			    ExprPtr	e;
			    Value	t;
			    ScopePtr	s;
			    FramePtr	f;

			    e = BuildCall ("Print",
					   7,
					   BuildName ("stdout"),
					   BuildCall ("HistoryInsert",
						      1, $1),
					   NewExprConst (NewStrString ("g")),
					   $3,
					   NewExprConst (Zero),
					   NewExprConst (NewInt (-1)),
					   NewExprConst (NewStrString (" ")));
			    e = NewExprTree (COMMA,
					     e,
					     BuildCall ("putchar",
							1,
							NewExprConst (NewInt ('\n'))));
			    GetScope (&s, &f);
			    t = NewThread (f, CompileExpr (e, s));
			    ThreadsRun (t, 0);
			    EXIT ();
			}
	|	stat { ignorenl = 0; } opt_nl 
		    { 
			ScopePtr    s;
			FramePtr    f;
			Value	    t;
			
			GetScope (&s, &f);
			t = NewThread (f, CompileStat ($1, s));
			ThreadsRun (t, 0);
		    }
	|	UNDEFINE uninit_names NL
			{
			    ENTER ();
			    DeclListPtr	dl;
			    SymbolPtr	s;
			    int		depth;

			    for (dl = $2; dl; dl = dl->next) {
				s = ScopeFindSymbol (GlobalScope,
						      dl->name,
						      &depth);
				if (!s)
				{
				    RaiseError (0, "Undefined symbol %A", dl->name);
				}
				else if (depth)
				{
				    RaiseError (0, "Can't undefine at higher scope %A", dl->name);
				}
				else
				{
				    ScopeRemoveSymbol (GlobalScope, s);
				}
			    }
			    EXIT ();
			}
	|	READ CONST NL
			{
			    ENTER ();
			    Value	filename;

			    filename = $2;
			    switch (filename->value.tag) {
			    default:
				yyerror ("non string filename %v", filename);
				break;
			    case type_string:
				pushinput (StringChars (&filename->string), True);
				break;
			    }
			    EXIT ();
			}
	|	HISTORY history NL
    			{
			    Value   t;
			    t = NewThread (0, CompileExpr ($2, GlobalScope));
			    ThreadsRun (t, 0);
			}
	|	PRINT NAME NL
			{ 
			    SymbolPtr	sym;
			    int		depth;
			    ScopePtr	s;
			    FramePtr	f;

			    GetScope (&s, &f);
			    sym = ScopeFindSymbol (s, $2, &depth);
			    if (!sym)
			    {
				yyerror ("Undefined symbol \"%s\"",
					 AtomName ($2));
			    }
			    else
				PrettyPrint (FileStdout, sym); 
			}
	|	EDIT edit {}
	|	NL {}
	;
opt_nl	:	NL {}
	|
	;
var_def	:	type_def ignorenl o_names
		    { 
			$$ = NewExprDecl ($3, $1.class, $1.type, $1.publish);
		    }
	;
o_names	:	names
	|
		    { $$ = 0; }
	;
names	:	NAME op_init COMMA names
		    { $$ = NewDeclList ($1, $2, $4); }
	|	NAME op_init
		    { $$ = NewDeclList ($1, $2, 0); }
	;
uninit_names:	NAME COMMA uninit_names
		    { $$ = NewDeclList ($1, 0, $3); }
	|	NAME
		    { $$ = NewDeclList ($1, 0, 0); }
	;
memlist	:	type uninit_names SEMI memlist
		{ $$ = NewMemList ($2, $1, $4); }
	|
		{ $$ = 0; }
	;
type_def:	class
		    { $$.class = $1; $$.type = type_undef; $$.publish = publish_private; }
	|	type
		    { $$.class = class_undef; $$.type = $1; $$.publish = publish_private; }
	|	publish
		    { $$.class = class_undef; $$.type = type_undef; $$.publish = $1; }
	|	type_def class
		    {
			$$ = $1;
			if ($1.class != class_undef)
			    yyerror ("Multiple class definitions \"%C\" \"%C\"",
				     $1.class, $2);
			$$.class = $2;
		    }
	|	type_def type
		    {
			$$ = $1;
			if ($1.type != type_undef)
			    yyerror ("Multiple type definitions \"%T\" \"%T\"",
				     $1.type, $2);
			$$.type = $2;
		    }
	|	type_def publish
		    {
			$$ = $1;    
			if ($1.publish != publish_private)
			    yyerror ("Multiple publish definitions \"%P\" \"%P\"",
				     $1.publish, $2);
			$$.publish = $2;
		    }
	;
o_type	:	type
		    { $$ = $1; }
	|
		    { $$ = type_undef; }
	;
type	:	INT
	|	NATURAL
	|	INTEGER
	|	RATIONAL
	|	DOUBLE
	|	STRING
	|	ARRAY
	|	REF
	|	STRUCT
	|	FUNC
	|	POLYMORPH
	|	STRUCT OC memlist CC o_publish NAME
		    {
			ENTER ();
			DeclListPtr	dl;
			SymbolPtr	s;
			StructType	*st;
			StructElement	*se;
			MemListPtr	ml;
			int		nelements;

			nelements = 0;
			for (ml = $3; ml; ml = ml->next)
			{
			    for (dl = ml->names; dl; dl = dl->next)
				nelements++;
			}
			st = NewStructType (nelements);
			se = StructTypeElements (st);
			nelements = 0;
			for (ml = $3; ml; ml = ml->next)
			{
			    for (dl = ml->names; dl; dl = dl->next)
			    {
				se[nelements].type = ml->type;
				se[nelements].name = dl->name;
				nelements++;
			    }
			}
			s = NewSymbolStruct ($6, st, $5);
			ScopeAddSymbol (GlobalScope, s);
			EXIT ();
		    }
	;
class	:	GLOBAL
	|	AUTO
	|	STATIC
	;
o_publish:	publish
		    { $$ = $1; }
	|
		    { $$ = publish_private; }
	;
publish	:	PUBLIC
	;
op_init	:	ASSIGN sexpr
		    { $$ = $2; }
	|
		    { $$ = 0; }
	;
ofargs	:	fargs
		    { $$ = $1; }
	|
		    { $$ = 0; }
	;
fargs	:	farg COMMA fargs
		    { $$ = NewExprTree (COMMA, $1, $3); }
	|	farg
		    { $$ = NewExprTree (COMMA, $1, 0); }
	;
farg	:	o_type NAME
		    { $$ = NewExprDecl (NewDeclList ($2, NULL, NULL),
					class_arg, $1, publish_private);
		    }
	;
stat	:	IF ignorenl OP expr CP stat
			{ $$ = NewExprTree(IF, $4, $6); }
	|	IF ignorenl OP expr CP stat ELSE stat
			{ 
			    $$ = NewExprTree(ELSE, $4, 
					     NewExprTree(ELSE, $6, $8)); 
			}
	|	WHILE ignorenl OP expr CP stat
			{ $$ = NewExprTree(WHILE, $4, $6); }
	|	DO ignorenl stat WHILE OP expr CP
			{ $$ = NewExprTree(DO, $3, $6); }
	|	FOR ignorenl OP optexpr SEMI optexpr SEMI optexpr CP stat
			{
			    $$ = NewExprTree(FOR, NewExprTree(FOR, $4, $6),
					     NewExprTree(FOR, $8, $10));
			}
	|	BREAK ignorenl SEMI
			{ $$ = NewExprTree(BREAK, (Expr *) 0, (Expr *) 0); }
	|	CONTINUE ignorenl SEMI
			{ $$ = NewExprTree(CONTINUE, (Expr *) 0, (Expr *) 0); }
	|	RETURNTOK ignorenl optexpr SEMI
			{ $$ = NewExprTree (RETURNTOK, (Expr *) 0, $3); }
	|	expr SEMI
			{ $$ = NewExprTree(EXPR, $1, (Expr *) 0); }
	|	OC ignorenl statlist CC
			{ $$ = $3; }
	|	SEMI ignorenl
			{ $$ = NewExprTree(SEMI, (Expr *) 0, (Expr *) 0); }
		/* This is just convenient shorthand for foo = function ... */
	|	FUNCTION ignorenl o_publish NAME fbody
			{ 
			    $$ = NewExprTree (FUNCTION,
					      NewExprDecl (NewDeclList ($4,
									0, 0),
							   class_undef,
							   type_func,
							   $3),
					      NewExprTree (ASSIGN,
							   NewExprAtom ($4),
							   NewExprCode ($5, 
									$4)));
			}
	|	var_def SEMI
			{ $$ = $1; }
	|	SCOPE ignorenl o_publish NAME OC statlist CC
			{
			    $$ = NewExprTree (SCOPE,
					      NewExprDecl (NewDeclList ($4, 
									0, 0),
							   class_undef,
							   type_undef,
							   $3),
					      $6);
			}
	|	IMPORT o_publish NAME SEMI
			{
			    $$ = NewExprTree (IMPORT,
					      NewExprDecl (NewDeclList ($3,
									0, 0),
							   class_undef,
							   type_undef,
							   $2),
					      0);
			}
	|	TRY ignorenl stat catches
			{ $$ = NewExprTree (TRY, $3, $4); }
	;
catch	:	CATCH NAME OP ofargs CP stat
			{
			    $$ = NewExprTree (CATCH, 
					      NewExprTree (EXCEPTION,
							   NewExprAtom ($2),
							   $4),
					      $6); 
			}
	;
catches	:	catch catches
			{ $$ = NewExprTree (TRY, $1, $2); }
	|	catch
			{ $$ = NewExprTree (TRY, $1, 0); }
	;
ignorenl:	{ ignorenl = 1; }
fbody	:	o_type OP ofargs CP OC statlist CC
		    { $$ = NewFuncCode ($1, $3, $6);}
	;
history	:	
		{ $$ = BuildCall ("HistoryShow", 1,
				  BuildName ("format")); }
	|	sexpr
		{ $$ = BuildCall ("HistoryShow", 2, 
				  BuildName ("format"), $1); }
	|	sexpr COMMA sexpr
		{ $$ = BuildCall ("HistoryShow", 3, 
				  BuildName ("format"), $1, $3); }
	|	ASSIGN expr
		{ $$ = BuildCall ("HistorySet", 1, $2); }
	;
edit	:	NAME NL
		{
		    SymbolPtr	s;
		    int		depth;

		    s = ScopeFindSymbol (GlobalScope, $1, &depth);
		    if (!s)
		    {
			yyerror ("Undefined symbol \"%A\"", $1);
		    }
		    else
			EditFunction (s);
		}
    	|	CONST NL
		{
		    ENTER ();
		    EditFile ($1);
		    EXIT ();
		}
	|	NL
		{
		    EditFile ((Value) 0);
		}
	;
optexpr	:	expr
			{ $$ = $1; }
	|
			{ $$ = 0; }
	;
statlist:	stat statlist
			{ $$ = NewExprTree(OC, $1, $2); }
	|
			{ $$ = NewExprTree(OC, 0, 0); }
	;
expr	:	sexpr
	|	expr COMMA sexpr
			{ $$ = NewExprTree(COMMA, $1, $3); }
	;
sexpr	:	primary
	|	MOD CONST					%prec THREAD
			{   Value	t;
			    t = ThreadFromId ($2);
			    if (Zerop (t))
			    {
				yyerror ("No thread %v", $2);
				YYERROR;
			    }
			    else
				$$ = NewExprConst(t); 
			}
	|	TIMES sexpr					%prec STAR
			{ $$ = NewExprTree(STAR, $2, (Expr *) 0); }
	|	LAND sexpr					%prec AMPER
			{ $$ = NewExprTree(AMPER, $2, (Expr *) 0); }
	|	MINUS sexpr					%prec UMINUS
			{ $$ = NewExprTree(UMINUS, $2, (Expr *) 0); }
	|	LNOT sexpr
			{ $$ = NewExprTree(LNOT, $2, (Expr *) 0); }
	|	BANG sexpr
			{ $$ = NewExprTree(BANG, $2, (Expr *) 0); }
	|	sexpr BANG					%prec FACT
			{ $$ = NewExprTree(FACT, $1, (Expr *) 0); }
	|	INC sexpr
			{ $$ = NewExprTree(INC, $2, (Expr *) 0); }
	|	sexpr INC
			{ $$ = NewExprTree(INC, (Expr *) 0, $1); }
	|	DEC sexpr
			{ $$ = NewExprTree(DEC, $2, (Expr *) 0); }
	|	sexpr DEC
			{ $$ = NewExprTree(DEC, (Expr *) 0, $1); }
	|	sexpr PLUS sexpr
			{
			binop:
				$$ = NewExprTree($2, $1, $3);
			}
	|	sexpr MINUS sexpr
			{ goto binop; }
	|	sexpr TIMES sexpr
			{ goto binop; }
	|	sexpr DIVIDE sexpr
			{ goto binop; }
	|	sexpr DIV sexpr
			{ goto binop; }
	|	sexpr MOD sexpr
			{ goto binop; }
	|	sexpr POW sexpr
			{ goto binop; }
	|	sexpr QUEST sexpr COLON sexpr
			{ $$ = NewExprTree(QUEST, $1, NewExprTree(COLON, $3, $5)); }
	|	sexpr LXOR sexpr
			{ goto binop; }
	|	sexpr LAND sexpr
			{ goto binop; }
	|	sexpr LOR sexpr
			{ goto binop; }
	|	sexpr AND sexpr
			{ goto binop; }
	|	sexpr OR sexpr
			{ goto binop; }
	|	sexpr ASSIGN sexpr
			{ goto binop; }
	|	sexpr assignop sexpr			%prec ASSIGN
			{ goto binop; }
	|	sexpr EQ sexpr
			{ goto binop; }
	|	sexpr NE sexpr
			{ goto binop; }
	|	sexpr LT sexpr
			{ goto binop; }
	|	sexpr GT sexpr
			{ goto binop; }
	|	sexpr LE sexpr
			{ goto binop; }
	|	sexpr GE sexpr
			{ goto binop; }
	;
primary	:	NAME
			{ $$ = NewExprAtom ($1); }
	|	CONST
			{ $$ = NewExprConst($1); }
	|	DOLLAR opt_const
			{ $$ = NewExprConst(History (0, $2)); }
	|    	DOT
			{ $$ = NewExprConst(History (0, Zero)); }
	|	OP expr CP
			{ $$ = $2; }
	|	primary OS arglist CS
			{ $$ = NewExprTree(OS, $1, $3); }
	|	primary OP oarglist CP				%prec CALL
			{ $$ = NewExprTree ($2, $1, $3); }
	|	FORK OP expr CP					%prec CALL
			{ $$ = NewExprTree (FORK, (Expr *) 0, $3); }
    	|	primary DOT NAME
			{ $$ = NewExprTree(DOT, $1, NewExprAtom ($3)); }
    	|	primary ARROW NAME
			{ $$ = NewExprTree(ARROW, $1, NewExprAtom ($3)); }
	|	FUNCTION fbody
			{ $$ = NewExprCode ($2, 0); }
	|	NAME OC sinits CC
			{ $$ = NewExprTree (STRUCT, NewExprAtom ($1), $3); }
	|	array oainits
			{ $$ = NewExprTree (ARRAY, $1, $2); }
	|	OS CS OC ainits CC
			{ $$ = NewExprTree (ARRAY, 0, 
					    NewExprTree (AINIT, $4, 0)); 
			}
	;
opt_const:	CONST
			{ $$ = $1; };
	|
			{ $$ = Zero; }
	;
assignop:	PLUS ASSIGN
			{ $$ = ASSIGNPLUS; }
	|	MINUS ASSIGN
			{ $$ = ASSIGNMINUS; }
	|	TIMES ASSIGN
			{ $$ = ASSIGNTIMES; }
	|	DIVIDE ASSIGN
			{ $$ = ASSIGNDIVIDE; }
	|	DIV ASSIGN
			{ $$ = ASSIGNDIV; }
	|	MOD ASSIGN
			{ $$ = ASSIGNMOD; }
	|	POW ASSIGN
			{ $$ = ASSIGNPOW; }
	|	LXOR ASSIGN
			{ $$ = ASSIGNLXOR; }
	|	LAND ASSIGN
			{ $$ = ASSIGNLAND; }
	|	LOR ASSIGN
			{ $$ = ASSIGNLOR; }
	;
array	:	OS arglist CS
			{ $$ = $2; }
	;
oainits	:	OC ainits CC
			{ $$ = NewExprTree (AINIT, $2, 0); }
	|
			{ $$ = 0; }
	;
aelem	:	sexpr
			{ $$ = $1; }
	|	OC ainits CC
			{ $$ = NewExprTree (AINIT, $2, 0); }
	;
ainits	:	aelem COMMA ainits
			{ $$ = NewExprTree (COMMA, $1, $3); }
	|	aelem
			{ $$ = NewExprTree (COMMA, $1, 0); }
	;
sinit	:	NAME ASSIGN sexpr
			{ $$ = NewExprTree (ASSIGN, NewExprAtom ($1), $3); }
	;
sinits	:	sinit COMMA sinits
			{ $$ = NewExprTree (COMMA, $1, $3); }
	|	sinit
			{ $$ = NewExprTree (COMMA, $1, 0); }
	;
oarglist:	arglist
	|
			{ $$ = 0; }
	;
arglist	:	arglist COMMA sexpr
			{ $$ = NewExprTree ($2, $3, $1); }
	|	sexpr
			{ $$ = NewExprTree (COMMA, $1, (Expr *) 0); }
	;
%%

# include	<stdio.h>

ScopePtr    CurrentScope;
FramePtr    CurrentFrame;

Value
lookupVar (char *name)
{
    ENTER ();
    SymbolPtr	s;
    int		depth;
    Value	v;

    s = ScopeFindSymbol (CurrentScope, AtomId (name), &depth);
    if (s && s->symbol.class == class_global)
	v = BoxValue (s->global.value, 0);
    else
	v = Zero;
    RETURN (v);
}

void
setVar (char *name, Value v)
{
    ENTER ();
    Atom	n;
    SymbolPtr	s;
    int		depth;

    n = AtomId (name);
    s = ScopeFindSymbol (CurrentScope, n, &depth);
    if (!s)
	s = ScopeAddSymbol (CurrentScope, NewSymbolGlobal (n, type_undef,
							  publish_private));
    if (s->symbol.class == class_global)
	BoxValue (s->global.value, 0) = v;
    EXIT ();
}

void
GetScope (ScopePtr *scope, FramePtr *fp)
{
    *scope = CurrentScope;
    *fp = CurrentFrame;
}

ExprPtr
BuildName (char *name)
{
    ENTER ();
    RETURN (NewExprAtom (AtomId(name)));
}

ExprPtr
BuildCall (char *name, int nargs, ...)
{
    ENTER ();
    va_list alist;
    ExprPtr args;
    ExprPtr e;

    va_start (alist, nargs);
    args = 0;
    while (nargs--)
    {
	args = NewExprTree (COMMA, va_arg (alist, ExprPtr), args);
    }
    va_end (alist);
    e = NewExprTree (OP, BuildName (name), args);
#ifdef DEBUG
    printExpr (stdout, e, -1, 0);
    printf ("\n");
#endif
    RETURN (e);
}

int
yywrap (void)
{
    if (interactive)
	printf ("\n");
    return 1;
}

void
yyerror (char *fmt, ...)
{
    va_list	args;
    extern char	*yyfile;
    extern int	yylineno;

    if (yyfiledeep)
	fprintf (stderr, "\"%s\": line %d, ", yyfile, yylineno);
    va_start (args, fmt);
    vPrintError (fmt, args);
    va_end (args);
}
