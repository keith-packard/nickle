%{
/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	<math.h>
#include	"nickle.h"
#include	<stdio.h>

int ignorenl;
extern int	yyfiledeep;

void yyerror (char *fmt, ...);

%}

%union {
    int		    ival;
    Value	    vval;
    Class	    cval;
    Type	    tval;
    ArgType	    *atval;
    Types	    *tsval;
    Publish	    pval;
    ExprPtr	    eval;
    Atom	    aval;
    DeclListPtr	    dval;
    MemListPtr	    mval;
    Fulltype	    ftval;
    ArgDecl	    adval;
    ScopePtr	    sval;
    CodePtr	    fval;
}

%type  <eval>	history
%type  <eval>	block statements statement
%type  <dval>	names typenames opt_initnames initnames
%type  <aval>	typename
%type  <eval>	opt_init
%type  <ftval>	decl opt_decl
%type  <tsval>	opt_type type
%type  <tval>	basetype
%type  <mval>	members
%type  <cval>	class
%type  <pval>	publish

%type  <atval>	opt_argdecls argdecls
%type  <adval>	argdecl
%type  <atval>	opt_argdefines argdefines
%type  <adval>	argdefine

%type  <eval>	opt_expr expr opt_exprs exprs lambdaexpr simpleexpr primary
%type  <eval>	initexpr
%type  <ival>	assignop
%type  <vval>	opt_const
%type  <eval>	inits arrayinits arrayinit structinits structinit

%token		ASSIGNPLUS ASSIGNMINUS ASSIGNTIMES ASSIGNDIVIDE ASSIGNDIV ASSIGNMOD
%token		ASSIGNPOW ASSIGNLXOR ASSIGNLAND ASSIGNLOR
%token		VAR EXPR ARRAY STRUCT

%token		NL SEMI MOD OC CC DOLLAR
%token		UNDEFINE READ HISTORY PRINT EDIT QUIT
%token <cval>	GLOBAL AUTO STATIC
%token <tval>	POLY INT INTEGER NATURAL RATIONAL DOUBLE STRING
%token <tval>	FILET MUTEX SEMAPHORE CONTINUATION
%token		FUNCTION FUNC
%token		TYPEDEF IMPORT SCOPE NEW
%token		EXCEPTION
%token <pval>	PUBLIC
%token		IF ELSE WHILE DO FOR BREAK CONTINUE RETURNTOK FORK TRY CATCH
%token <aval>	NAME
%token <vval>	CONST
%token		NEW

%nonassoc 	POUND
%right		COMMA
%right		ASSIGN
%right		QUEST COLON
%left		OR
%left		AND
%left		LOR
%left		LXOR
%left		LAND
%left		EQ NE
%left		LT GT LE GE
%left		PLUS MINUS
%left		TIMES DIVIDE DIV MOD
%right		POW
%right		UMINUS BANG FACT LNOT INC DEC STAR AMPER THREAD
%left		OS CS DOT ARROW REFARRAY CALL OP CP

%%
lines		: lines pcommand
		|
		;
pcommand	: command
		| error attendnl eend
		;
eend		: NL
		    { yyerrok; }
		| SEMI
		    { yyerrok; }
		;

/*
 * Sprinkled through the grammer to switch the lexer between reporting
 * and not reporting newlines
 */
ignorenl	:
		    { ignorenl = 1; }
		;

attendnl	:
		    { ignorenl = 0; }
		;
opt_nl		: NL
		|
		;
/*
 * Interpreter command level
 */
command		: QUIT NL
		    { YYACCEPT; }
		| expr NL
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
		| expr POUND expr NL
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
		| statement attendnl opt_nl
		    { 
			ScopePtr    s;
			FramePtr    f;
			Value	    t;
			
			GetScope (&s, &f);
			t = NewThread (f, CompileStat ($1, s));
			ThreadsRun (t, 0);
		    }
		| UNDEFINE names NL
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
		| READ CONST NL
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
		| HISTORY history attendnl NL
    		    {
			Value   t;
			t = NewThread (0, CompileExpr ($2, GlobalScope));
			ThreadsRun (t, 0);
		    }
		| PRINT NAME NL
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
		| EDIT edit
		| NL
		;
history		: 
		    { 
			$$ = BuildCall ("HistoryShow", 1, BuildName ("format")); 
		    }
		| simpleexpr
		    { 
			$$ = BuildCall ("HistoryShow", 2, BuildName ("format"), $1); 
		    }
		| simpleexpr COMMA simpleexpr
		    { 
			$$ = BuildCall ("HistoryShow", 3, BuildName ("format"), $1, $3);
		    }
		| ASSIGN expr
		    { 
			$$ = BuildCall ("HistorySet", 1, $2); 
		    }
		;
edit		: NAME NL
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
		| CONST NL
		    { ENTER (); EditFile ($1); EXIT (); }
		| NL
		    { EditFile ((Value) 0); }
		;
/*
* Statements
*/
block		: OC ignorenl statements CC
		    { $$ = $3; }
		;
statements	: statement statements
		    { $$ = NewExprTree(OC, $1, $2); }
		|
		    { $$ = NewExprTree(OC, 0, 0); }
		;
statement	: IF ignorenl OP expr CP statement
		    { $$ = NewExprTree(IF, $4, $6); }
		| IF ignorenl OP expr CP statement ELSE statement
		    { $$ = NewExprTree(ELSE, $4, NewExprTree(ELSE, $6, $8)); }
		| WHILE ignorenl OP expr CP statement
		    { $$ = NewExprTree(WHILE, $4, $6); }
		| DO ignorenl statement WHILE OP expr CP
		    { $$ = NewExprTree(DO, $3, $6); }
		| FOR ignorenl OP opt_expr SEMI opt_expr SEMI opt_expr CP statement
		    { $$ = NewExprTree(FOR, NewExprTree(FOR, $4, $6),
				       NewExprTree(FOR, $8, $10));
		    }
		| BREAK ignorenl SEMI
		    { $$ = NewExprTree(BREAK, (Expr *) 0, (Expr *) 0); }
		| CONTINUE ignorenl SEMI
		    { $$ = NewExprTree(CONTINUE, (Expr *) 0, (Expr *) 0); }
		| RETURNTOK ignorenl opt_expr SEMI
		    { $$ = NewExprTree (RETURNTOK, (Expr *) 0, $3); }
		| expr SEMI
		    { $$ = NewExprTree(EXPR, $1, (Expr *) 0); }
		| block
		| SEMI ignorenl
		    { $$ = NewExprTree(SEMI, (Expr *) 0, (Expr *) 0); }
		| opt_decl FUNCTION ignorenl NAME OP opt_argdefines CP block
		    { 
			$$ = NewExprTree (FUNCTION,
					  NewExprDecl (NewDeclList ($4, 0, 0),
						       $1.class,
							NewTypesFunc ($1.type, False, $6),
						       $1.publish),
					  NewExprTree (ASSIGN,
						       NewExprAtom ($4),
						       NewExprCode (NewFuncCode ($1.type,
										 $6,
										 $8),
								    $4)));
		    }
		| decl ignorenl opt_initnames SEMI
		    { $$ = NewExprDecl ($3, $1.class, $1.type, $1.publish); }
		| publish TYPEDEF ignorenl opt_type typenames SEMI
		    { $$ = NewExprDecl ($5, class_typedef, $4, $1); }
		| publish SCOPE ignorenl NAME block
		    {
			$$ = NewExprTree (SCOPE,
					  NewExprDecl (NewDeclList ($4, 
								    0, 0),
						       class_undef,
						       0,
						       $1),
					  $5);
		    }
		| publish IMPORT ignorenl NAME SEMI
		    {
			$$ = NewExprTree (IMPORT,
					  NewExprDecl (NewDeclList ($4,
								    0, 0),
						       class_undef,
						       0,
						       $1),
					  0);
		    }
		;

/*
* Identifiers
*/
names		: NAME COMMA names
		    { $$ = NewDeclList ($1, 0, $3); }
		| NAME
		    { $$ = NewDeclList ($1, 0, 0); }
		;
typenames	: typename COMMA typenames
		    { $$ = NewDeclList ($1, 0, $3); }
		| typename
		    { $$ = NewDeclList ($1, 0, 0); }
		;
typename	: COLON NAME
		    { $$ = $2; }
		;
opt_initnames	: initnames
		|
		    { $$ = 0; }
		;
initnames	: NAME opt_init COMMA initnames
		    { $$ = NewDeclList ($1, $2, $4); }
		| NAME opt_init
		    { $$ = NewDeclList ($1, $2, 0); }
		;
opt_init	: ASSIGN simpleexpr
		    { $$ = $2; }
/*
 * This will allow struct { int x; } bar = { x = 10 };
 */
/*		| ASSIGN inits
		    { $$ = $2; } */
		|
		    { $$ = 0; }
		;
/*
* Full declaration including storage, type and publication
*/
opt_decl	: PUBLIC class type
		    { $$.publish = $1; $$.class = $2; $$.type = $3; }
		| class type
		    { $$.publish = publish_private; $$.class = $1; $$.type = $2; }
		| PUBLIC type
		    { $$.publish = $1; $$.class = class_undef; $$.type = $2; }
		| type
		    { $$.publish = publish_private; $$.class = class_undef; $$.type = $1; }
		| PUBLIC class
		    { $$.publish = $1; $$.class = $2; $$.type = typesPoly; }
		| class
		    { $$.publish = publish_private; $$.class = $1; $$.type = typesPoly; }
		| PUBLIC
		    { $$.publish = $1; $$.class = class_undef; $$.type = typesPoly; }
		|
		    { $$.publish = publish_private; $$.class = class_undef; $$.type = typesPoly; }
		;
decl		: PUBLIC class type
		    { $$.publish = $1; $$.class = $2; $$.type = $3; }
		| class type
		    { $$.publish = publish_private; $$.class = $1; $$.type = $2; }
		| PUBLIC type
		    { $$.publish = $1; $$.class = class_undef; $$.type = $2; }
		| type
		    { $$.publish = publish_private; $$.class = class_undef; $$.type = $1; }
		;
/*
* Type declarations
*/
opt_type	: type
		|
		    { $$ = typesPoly; }
		;
type		: basetype
		    { $$ = NewTypesPrim ($1); }
		| TIMES type			%prec STAR
		    { $$ = NewTypesRef ($2); }
		| type OP opt_argdecls CP	%prec CALL
		    { $$ = NewTypesFunc ($1, False, $3); }
		| type OS opt_exprs CS
		    { $$ = NewTypesArray ($1, $3); }
		| STRUCT OC members CC
		    {
			DeclListPtr	dl;
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
			$$ = NewTypesStruct (st);
		    }
		| OP type CP
		    { $$ = $2; }
		| typename
		    { $$ = NewTypesName ($1, 0); }
		;
basetype    	: POLY
		| INT
		| INTEGER
		| RATIONAL
		| DOUBLE
		| STRING
		| FILET
		| MUTEX
		| SEMAPHORE
		| CONTINUATION
		;

/*
* Structure member declarations
*/
members		: opt_type names SEMI members
		    { $$ = NewMemList ($2, $1, $4); }
		|
		    { $$ = 0; }
		;

/*
* Declaration modifiers
*/
class		: GLOBAL
		| AUTO
		| STATIC
		;

publish		: PUBLIC
		|
		    { $$ = publish_private; }
		;

/*
* Arguments in function declarations
*/
opt_argdecls	: argdecls
		|
		    { $$ = 0; }
		;
argdecls	: argdecl COMMA argdecls
		    { $$ = NewArgType ($1.type, $1.name, $3); }
		| argdecl
		    { $$ = NewArgType ($1.type, $1.name, 0); }
		;
argdecl		: type NAME
		    { $$.type = $1; $$.name = $2; }
		| type
		    { $$.type = $1; $$.name = 0; }
		| NAME
		    { $$.type = typesPoly; $$.name = $1; }
		;

/*
* Arguments in function definitions
*/
opt_argdefines	: argdefines
		|
		    { $$ = 0; }
		;
argdefines	: argdefine COMMA argdefines
		    { $$ = NewArgType ($1.type, $1.name, $3); }
		| argdefine
		    { $$ = NewArgType ($1.type, $1.name, 0); }
		;
argdefine	: opt_type NAME
		    { $$.type = $1; $$.name = $2; }
		;

/*
* Expressions, top level includes comma operator
*/
opt_expr	: expr
		|
		    { $$ = 0; }
		;
expr		: lambdaexpr
		| expr COMMA lambdaexpr
		    { $$ = NewExprTree(COMMA, $1, $3); }
		;
/*
* Expression list, different use of commas for function arguments et al.
*/
opt_exprs	: exprs
		|
		    { $$ = 0; }
		;
exprs		: exprs COMMA lambdaexpr
		    { $$ = NewExprTree (COMMA, $3, $1); }
		| lambdaexpr
		    { $$ = NewExprTree (COMMA, $1, (Expr *) 0); }
		;

/*
* This expression level includes lambdas which can't be in simpleexpr
* because of grammar ambiguities
*/
lambdaexpr	: opt_type FUNC OP opt_argdefines CP block
		    { $$ = NewExprCode (NewFuncCode ($1, $4, $6), 0); }
		| simpleexpr
		;
/*
* Fundemental expression production
*/
simpleexpr	: primary
		| MOD CONST						%prec THREAD
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
		| TIMES simpleexpr					%prec STAR
		    { $$ = NewExprTree(STAR, $2, (Expr *) 0); }
		| LAND simpleexpr					%prec AMPER
		    { $$ = NewExprTree(AMPER, $2, (Expr *) 0); }
		| MINUS simpleexpr					%prec UMINUS
		    { $$ = NewExprTree(UMINUS, $2, (Expr *) 0); }
		| LNOT simpleexpr
		    { $$ = NewExprTree(LNOT, $2, (Expr *) 0); }
		| BANG simpleexpr
		    { $$ = NewExprTree(BANG, $2, (Expr *) 0); }
		| simpleexpr BANG					%prec FACT
		    { $$ = NewExprTree(FACT, $1, (Expr *) 0); }
		| INC simpleexpr
		    { $$ = NewExprTree(INC, $2, (Expr *) 0); }
		| simpleexpr INC
		    { $$ = NewExprTree(INC, (Expr *) 0, $1); }
		| DEC simpleexpr
		    { $$ = NewExprTree(DEC, $2, (Expr *) 0); }
		| simpleexpr DEC
		    { $$ = NewExprTree(DEC, (Expr *) 0, $1); }
		| simpleexpr PLUS simpleexpr
		    { $$ = NewExprTree(PLUS, $1, $3); }
		| simpleexpr MINUS simpleexpr
		    { $$ = NewExprTree(MINUS, $1, $3); }
		| simpleexpr TIMES simpleexpr
		    { $$ = NewExprTree(TIMES, $1, $3); }
		| simpleexpr DIVIDE simpleexpr
		    { $$ = NewExprTree(DIVIDE, $1, $3); }
		| simpleexpr DIV simpleexpr
		    { $$ = NewExprTree(DIV, $1, $3); }
		| simpleexpr MOD simpleexpr
		    { $$ = NewExprTree(MOD, $1, $3); }
		| simpleexpr POW simpleexpr
		    { $$ = NewExprTree(POW, $1, $3); }
		| simpleexpr QUEST simpleexpr COLON simpleexpr
		    { $$ = NewExprTree(QUEST, $1, NewExprTree(COLON, $3, $5)); }
		| simpleexpr LXOR simpleexpr
		    { $$ = NewExprTree(LXOR, $1, $3); }
		| simpleexpr LAND simpleexpr
		    { $$ = NewExprTree(LAND, $1, $3); }
		| simpleexpr LOR simpleexpr
		    { $$ = NewExprTree(LOR, $1, $3); }
		| simpleexpr AND simpleexpr
		    { $$ = NewExprTree(AND, $1, $3); }
		| simpleexpr OR simpleexpr
		    { $$ = NewExprTree(OR, $1, $3); }
		| simpleexpr assignop simpleexpr			%prec ASSIGN
		    { $$ = NewExprTree($2, $1, $3); }
		| simpleexpr EQ simpleexpr
		    { $$ = NewExprTree(EQ, $1, $3); }
		| simpleexpr NE simpleexpr
		    { $$ = NewExprTree(NE, $1, $3); }
		| simpleexpr LT simpleexpr
		    { $$ = NewExprTree(LT, $1, $3); }
		| simpleexpr GT simpleexpr
		    { $$ = NewExprTree(GT, $1, $3); }
		| simpleexpr LE simpleexpr
		    { $$ = NewExprTree(LE, $1, $3); }
		| simpleexpr GE simpleexpr
		    { $$ = NewExprTree(GE, $1, $3); }
		;
assignop	: PLUS ASSIGN
		    { $$ = ASSIGNPLUS; }
		| MINUS ASSIGN
		    { $$ = ASSIGNMINUS; }
		| TIMES ASSIGN
		    { $$ = ASSIGNTIMES; }
		| DIVIDE ASSIGN
		    { $$ = ASSIGNDIVIDE; }
		| DIV ASSIGN
		    { $$ = ASSIGNDIV; }
		| MOD ASSIGN
		    { $$ = ASSIGNMOD; }
		| POW ASSIGN
		    { $$ = ASSIGNPOW; }
		| LXOR ASSIGN
		    { $$ = ASSIGNLXOR; }
		| LAND ASSIGN
		    { $$ = ASSIGNLAND; }
		| LOR ASSIGN
		    { $$ = ASSIGNLOR; }
		| ASSIGN
		    { $$ = ASSIGN; }
		;
initexpr	: primary
		| MOD CONST						%prec THREAD
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
		| TIMES initexpr					%prec STAR
		    { $$ = NewExprTree(STAR, $2, (Expr *) 0); }
		| LAND initexpr					%prec AMPER
		    { $$ = NewExprTree(AMPER, $2, (Expr *) 0); }
		| MINUS initexpr					%prec UMINUS
		    { $$ = NewExprTree(UMINUS, $2, (Expr *) 0); }
		| LNOT initexpr
		    { $$ = NewExprTree(LNOT, $2, (Expr *) 0); }
		| BANG initexpr
		    { $$ = NewExprTree(BANG, $2, (Expr *) 0); }
		| initexpr BANG					%prec FACT
		    { $$ = NewExprTree(FACT, $1, (Expr *) 0); }
		| INC initexpr
		    { $$ = NewExprTree(INC, $2, (Expr *) 0); }
		| initexpr INC
		    { $$ = NewExprTree(INC, (Expr *) 0, $1); }
		| DEC initexpr
		    { $$ = NewExprTree(DEC, $2, (Expr *) 0); }
		| initexpr DEC
		    { $$ = NewExprTree(DEC, (Expr *) 0, $1); }
		| initexpr PLUS initexpr
		    { $$ = NewExprTree(PLUS, $1, $3); }
		| initexpr MINUS initexpr
		    { $$ = NewExprTree(MINUS, $1, $3); }
		| initexpr TIMES initexpr
		    { $$ = NewExprTree(TIMES, $1, $3); }
		| initexpr DIVIDE initexpr
		    { $$ = NewExprTree(DIVIDE, $1, $3); }
		| initexpr DIV initexpr
		    { $$ = NewExprTree(DIV, $1, $3); }
		| initexpr MOD initexpr
		    { $$ = NewExprTree(MOD, $1, $3); }
		| initexpr POW initexpr
		    { $$ = NewExprTree(POW, $1, $3); }
		| initexpr QUEST initexpr COLON initexpr
		    { $$ = NewExprTree(QUEST, $1, NewExprTree(COLON, $3, $5)); }
		| initexpr LXOR initexpr
		    { $$ = NewExprTree(LXOR, $1, $3); }
		| initexpr LAND initexpr
		    { $$ = NewExprTree(LAND, $1, $3); }
		| initexpr LOR initexpr
		    { $$ = NewExprTree(LOR, $1, $3); }
		| initexpr AND initexpr
		    { $$ = NewExprTree(AND, $1, $3); }
		| initexpr OR initexpr
		    { $$ = NewExprTree(OR, $1, $3); }
		| initexpr EQ initexpr
		    { $$ = NewExprTree(EQ, $1, $3); }
		| initexpr NE initexpr
		    { $$ = NewExprTree(NE, $1, $3); }
		| initexpr LT initexpr
		    { $$ = NewExprTree(LT, $1, $3); }
		| initexpr GT initexpr
		    { $$ = NewExprTree(GT, $1, $3); }
		| initexpr LE initexpr
		    { $$ = NewExprTree(LE, $1, $3); }
		| initexpr GE initexpr
		    { $$ = NewExprTree(GE, $1, $3); }
		;
primary		: NAME
		    { $$ = NewExprAtom ($1); }
		| CONST
		    { $$ = NewExprConst($1); }
		| type inits
		    { 
			$$ = NewExprTree (NEW, $2, 0); 
			$$->base.type = $1; 
		    }
		| DOLLAR opt_const
		    { $$ = NewExprConst(History (0, $2)); }
		| DOT
		    { $$ = NewExprConst(History (0, Zero)); }
		| OP expr CP
		    { $$ = $2; }
		| primary REFARRAY exprs CS
		    { $$ = NewExprTree (OS, NewExprTree (STAR, $1, (Expr *) 0), $3); }
		| primary OS exprs CS
		    { $$ = NewExprTree(OS, $1, $3); }
		| primary OP opt_exprs CP				%prec CALL
		    { $$ = NewExprTree (OP, $1, $3); }
		| FORK OP expr CP					%prec CALL
		    { $$ = NewExprTree (FORK, (Expr *) 0, $3); }
		| primary DOT NAME
		    { $$ = NewExprTree(DOT, $1, NewExprAtom ($3)); }
		| primary ARROW NAME
		    { $$ = NewExprTree(ARROW, $1, NewExprAtom ($3)); }
		;
opt_const	: CONST
		|
		    { $$ = Zero; }
		;
/*
 * Initializers
 */
inits		: OC arrayinits CC
		    { $$ = NewExprTree (ARRAY, $2, 0); }
		| OC structinits CC
		    { $$ = NewExprTree (STRUCT, $2, 0); }
		| OC CC
		    { $$ = 0; }
		;
/*
 * Array initializers
 */
arrayinits	: arrayinit COMMA arrayinits
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| arrayinit
		    { $$ = NewExprTree (COMMA, $1, 0); }
		;
arrayinit	: initexpr
		| OC arrayinits CC
		    { $$ = NewExprTree (ARRAY, $2, 0); }
		;

/* 
* Structure initializers
*/
structinits	: structinit COMMA structinits
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| structinit
		    { $$ = NewExprTree (COMMA, $1, 0); }
		;
structinit	: NAME ASSIGN initexpr
		    { $$ = NewExprTree (ASSIGN, NewExprAtom ($1), $3); }
		;

%%

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
    MemReference (ml->type);
    MemReference (ml->names);
}

DataType MemListType = { MemListMark, 0 };

MemListPtr
NewMemList (DeclListPtr names, Types *type, MemListPtr next)
{
    ENTER ();
    MemListPtr	ml;

    ml = ALLOCATE (&MemListType, sizeof (MemList));
    ml->type = type;
    ml->names = names;
    ml->next = next;
    RETURN (ml);
}


extern ScopePtr	CurrentScope;
FramePtr	CurrentFrame;

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
	s = ScopeAddSymbol (CurrentScope, NewSymbolGlobal (n, 0,
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
