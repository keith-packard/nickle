%{
/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

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
    NamespacePtr    nsval;
    CodePtr	    fval;
    Bool	    bval;
}

%type  <nsval>	colonnames
%type  <eval>	history
%type  <eval>	block func_body statements statement catches catch
%type  <eval>	case_block cases case
%type  <eval>	union_case_block union_cases union_case
%type  <dval>	enames names opt_initnames initnames
%type  <aval>	typename name
%type  <eval>	opt_init
%type  <ftval>	decl func_decl
%type  <tsval>	opt_type type type_or_void func_type
%type  <eval>   opt_stars stars
%type  <tval>	basetype
%type  <tval>	struct_or_union
%type  <mval>	struct_members
%type  <cval>	class
%type  <pval>	publish publish_extend

%type  <atval>	opt_argdecls argdecls
%type  <adval>	argdecl
%type  <atval>	opt_argdefines argdefines
%type  <adval>	argdefine
%type  <bval>	opt_dots

%type  <eval>	opt_expr expr opt_exprs exprs lambdaexpr simpleexpr primary
%type  <eval>	initexpr
%type  <ival>	assignop
%type  <vval>	opt_const const
%type  <eval>	opt_inits inits arrayinits arrayinit structinits structinit

%token		VAR EXPR ARRAY STRUCT UNION

%token		NL SEMI MOD OC CC DOLLAR DOTS
%token		UNDEFINE LOAD HISTORY PRINT EDIT QUIT
%token <cval>	GLOBAL AUTO STATIC
%token <tval>	POLY INTEGER NATURAL RATIONAL REAL STRING
%token <tval>	FILET MUTEX SEMAPHORE CONTINUATION THREAD
%token		VOID FUNCTION FUNC EXCEPTION RAISE
%token		TYPEDEF IMPORT NAMESPACE NEW
%token <pval>	PUBLIC EXTEND
%token		IF ELSE WHILE DO FOR SWITCH
%token		BREAK CONTINUE RETURNTOK FORK CASE DEFAULT
%token		TRY CATCH TWIXT
%token <aval>	NAME TYPENAME
%token <vval>	CONST CCONST

%nonassoc 	POUND
%right		COMMA
%right <ival>	ASSIGN
		ASSIGNPLUS ASSIGNMINUS ASSIGNTIMES ASSIGNDIVIDE 
		ASSIGNDIV ASSIGNMOD ASSIGNPOW
		ASSIGNSHIFTL ASSIGNSHIFTR
		ASSIGNLXOR ASSIGNLAND ASSIGNLOR
%right		QUEST COLON
%left		OR
%left		AND
%left		LOR
%left		LXOR
%left		LAND
%left		EQ NE
%left		LT GT LE GE
%left		SHIFTL SHIFTR
%left		PLUS MINUS
%left		TIMES DIVIDE DIV MOD
%right		POW
%left		UNIONCAST
%right		UMINUS BANG FACT LNOT INC DEC STAR AMPER THREADID
%left		OS CS DOT ARROW REFARRAY CALL OP CP
%right		COLONCOLON
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
			NamespacePtr	s;
			FramePtr	f;
			
			e = NewExprTree (COMMA,
					 BuildCall (0, "printf",
						    2,
						    BuildName ("format"),
						    BuildCall ("History",
							       "insert",
							       1, 
							       NewExprTree (DOLLAR, $1, 0))),
					 BuildCall (0, "putchar",
						    1,
						    NewExprConst (CCONST, NewInt ('\n'))));
			GetNamespace (&s, &f);
			t = NewThread (f, CompileExpr (e, s));
			ThreadsRun (t, 0);
			EXIT ();
		    }
		| expr POUND expr NL
		    {
			ENTER ();
			ExprPtr	e;
			Value	t;
			NamespacePtr	s;
			FramePtr	f;

			e = BuildCall ("File", "print",
				       7,
				       BuildName ("stdout"),
				       BuildCall ("History", "insert",
						  1, $1),
				       NewExprConst (CONST, NewStrString ("g")),
				       $3,
				       NewExprConst (CONST, Zero),
				       NewExprConst (CONST, NewInt (-1)),
				       NewExprConst (CONST, NewStrString (" ")));
			e = NewExprTree (COMMA,
					 e,
					 BuildCall (0, "putchar",
						    1,
						    NewExprConst (CCONST, NewInt ('\n'))));
			GetNamespace (&s, &f);
			t = NewThread (f, CompileExpr (e, s));
			ThreadsRun (t, 0);
			EXIT ();
		    }
		| statement attendnl opt_nl
		    { 
			NamespacePtr    s;
			FramePtr    f;
			Value	    t;
			
			GetNamespace (&s, &f);
			t = NewThread (f, CompileStat ($1, s));
			ThreadsRun (t, 0);
		    }
		| UNDEFINE enames NL
		    {
			ENTER ();
			DeclListPtr	dl;
			SymbolPtr	s;
			int		depth;

			for (dl = $2; dl; dl = dl->next) {
			    s = NamespaceFindSymbol (GlobalNamespace,
						     dl->name,
						     &depth);
			    if (!s)
			    {
				RaiseError ("Undefined symbol %A", dl->name);
			    }
			    else if (depth)
			    {
				RaiseError ("Can't undefine at higher scope %A", dl->name);
			    }
			    else
			    {
				CurrentTypespace = TypespaceRemove (CurrentTypespace,
								    dl->name);
				NamespaceRemoveSymbol (GlobalNamespace, s);
			    }
			}
			EXIT ();
		    }
		| LOAD const NL
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
			t = NewThread (0, CompileExpr ($2, GlobalNamespace));
			ThreadsRun (t, 0);
		    }
		| PRINT colonnames name NL
		    { 
			SymbolPtr	sym;
			int		depth;

			if ($2)
			{
			    sym = NamespaceFindSymbol ($2, $3, &depth);
			    if (!sym)
			    {
				yyerror ("Undefined symbol \"%s\"", AtomName ($3));
			    }
			    else
				PrettyPrint (FileStdout, sym); 
			}
		    }
		| EDIT edit
		| NL
		;
colonnames	: colonnames name COLONCOLON
		    {
			SymbolPtr   sym;
			int	    depth;

			if ($1)
			{
			    sym = NamespaceFindSymbol ($1, $2, &depth);
			    if (!sym)
			    {
				yyerror ("Undefined symbol \"%s\"", AtomName($2));
				$$ = 0;
			    }
			    else if (sym->symbol.class != class_namespace)
			    {
				yyerror ("\"%s\" does not reference a namespace", AtomName($2));
				$$ = 0;
			    }
			    else
				$$ = sym->namespace.namespace;
			}
		    }
		|
		    {	
			NamespacePtr    s;
			FramePtr    f;
			GetNamespace (&s, &f);
			$$ = s;
		    }
		;
history		: 
		    { 
			$$ = BuildCall ("History", "show", 1, BuildName ("format")); 
		    }
		| simpleexpr
		    { 
			$$ = BuildCall ("History", "show", 2, BuildName ("format"), $1); 
		    }
		| simpleexpr COMMA simpleexpr
		    { 
			$$ = BuildCall ("History", "show", 3, BuildName ("format"), $1, $3);
		    }
		;
edit		: name NL
		    {
			SymbolPtr	s;
			int		depth;

			s = NamespaceFindSymbol (GlobalNamespace, $1, &depth);
			if (!s)
			{
			    yyerror ("Undefined symbol \"%A\"", $1);
			}
			else
			    EditFunction (s);
		    }
		| const NL
		    { ENTER (); EditFile ($1); EXIT (); }
		| NL
		    { EditFile ((Value) 0); }
		;
/*
* Statements
*/
block		: block_start ignorenl statements block_end
		    { $$ = $3; }
		;
block_start	: OC
		    { 
			CurrentTypespace = NewTypespace (CurrentTypespace, 
							 0, False); 
		    }
		;
block_end	: CC
		    { 
			CurrentTypespace = TypespaceFind (CurrentTypespace, 
							  0)->previous; 
		    }
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
		| SWITCH ignorenl OP expr CP case_block
		    { $$ = NewExprTree (SWITCH, $4, $6); }
		| UNION SWITCH ignorenl OP expr CP union_case_block
		    { $$ = NewExprTree (UNION, $5, $7); }
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
		| func_decl FUNCTION ignorenl NAME OP opt_argdefines CP func_body
		    { 
			ExprPtr	decl;

			decl = NewExprDecl (NewDeclList ($4, 0, 0),
					    $1.class,
					    NewTypesFunc ($1.type, $6),
					    $1.publish);
			if ($8)
			    $$ = NewExprTree (FUNCTION, decl,
					      NewExprTree (ASSIGN,
							   NewExprAtom ($4),
							   NewExprCode (NewFuncCode ($1.type,
										     $6,
										     $8),
									$4)));
			else
			    $$ = decl;
		    }
		| publish EXCEPTION ignorenl NAME OP opt_argdefines CP SEMI
		    { $$ = NewExprDecl (NewDeclList ($4, 0, 0),
					class_exception,
					NewTypesFunc (typesPoly, $6),
					$1);
		    }
		| RAISE NAME OP opt_exprs CP SEMI;
		    { $$ = NewExprTree (RAISE, NewExprAtom ($2), $4); }
		| decl ignorenl opt_initnames SEMI
		    { $$ = NewExprDecl ($3, $1.class, $1.type, $1.publish); }
		| publish TYPEDEF ignorenl enames SEMI
		    { 
			DeclListPtr dl;
			
			$$ = NewExprDecl ($4, class_typedef, 0, $1);
			for (dl = $4; dl; dl = dl->next)
			    CurrentTypespace = NewTypespace (CurrentTypespace,
							     dl->name,
							     False);
		    }
		| publish TYPEDEF ignorenl type enames SEMI
		    { 
			DeclListPtr dl;
			
			$$ = NewExprDecl ($5, class_typedef, $4, $1);
			for (dl = $5; dl; dl = dl->next)
			    CurrentTypespace = NewTypespace (CurrentTypespace,
							     dl->name,
							     False);
		    }
		| publish_extend NAMESPACE ignorenl NAME block
		    {
			$$ = NewExprTree (NAMESPACE,
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
		| TRY ignorenl statement catches
		    { $$ = NewExprTree (CATCH, $3, $4); }
		| TWIXT ignorenl OP opt_expr SEMI opt_expr CP statement
		    { $$ = NewExprTree (TWIXT, 
					NewExprTree (TWIXT, $4, $6),
					NewExprTree (TWIXT, $8, 0));
		    }
		| TWIXT ignorenl OP opt_expr SEMI opt_expr CP statement ELSE statement
		    { $$ = NewExprTree (TWIXT, 
					NewExprTree (TWIXT, $4, $6),
					NewExprTree (TWIXT, $8, $10));
		    }
		;
catches		:   catch catches
		    { $$ = NewExprTree (CATCH, $1, $2); }
		|   
		    { $$ = 0; }
		;
catch		: CATCH NAME OP opt_argdefines CP block
		    { $$ = NewExprCode (NewFuncCode (typesPoly, $4, $6), $2); }
func_body    	: block
		| SEMI
		    { $$ = 0; }
		;
case_block	: block_start cases block_end
		    { $$ = $2; }
		;
cases		: case cases
		    { $$ = NewExprTree (CASE, $1, $2); }
		|
		    { $$ = 0; }
		;
case		: CASE expr COLON statements
		    { $$ = NewExprTree (CASE, $2, $4); }
		| DEFAULT COLON statements
		    { $$ = NewExprTree (CASE, 0, $3); }
		;
union_case_block: block_start union_cases block_end
		    { $$ = $2; }
		;
union_cases	: union_case union_cases
		    { $$ = NewExprTree (CASE, $1, $2); }    
		|
		    { $$ = 0; }
		;
union_case	: CASE NAME COLON statements
		    { $$ = NewExprTree (CASE, NewExprAtom ($2), $4); }
		| DEFAULT COLON statements
		    { $$ = NewExprTree (CASE, 0, $3); }
		;
/*
* Identifiers
*/
enames		: name COMMA enames
		    { $$ = NewDeclList ($1, 0, $3); }
		| name
		    { $$ = NewDeclList ($1, 0, 0); }
		;
name		: NAME
		| TYPENAME
		;
names		: NAME COMMA names
		    { $$ = NewDeclList ($1, 0, $3); }
		| NAME
		    { $$ = NewDeclList ($1, 0, 0); }
		;
typename	: TYPENAME
		;
opt_initnames	: initnames
		|
		    { $$ = 0; }
		;
initnames	: name opt_init COMMA initnames
		    { $$ = NewDeclList ($1, $2, $4); }
		| name opt_init
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
func_decl	: PUBLIC class type_or_void
		    { $$.publish = $1; $$.class = $2; $$.type = $3; }
		| class type_or_void
		    { $$.publish = publish_private; $$.class = $1; $$.type = $2; }
		| PUBLIC type_or_void
		    { $$.publish = $1; $$.class = class_undef; $$.type = $2; }
		| type_or_void
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
type_or_void	: type
		| VOID
		    { $$ = 0; }
		;
func_type	: type
		| VOID
		    { $$ = 0; }
		|
		    { $$ = typesPoly; }
		;
type		: basetype
		    { $$ = NewTypesPrim ($1); }
		| TIMES type			%prec STAR
		    { $$ = NewTypesRef ($2); }
		| type OP opt_argdecls CP	%prec CALL
		    { $$ = NewTypesFunc ($1, $3); }
		| VOID OP opt_argdecls CP	%prec CALL
		    { $$ = NewTypesFunc (0, $3); }
		| type OS opt_stars CS
		    { $$ = NewTypesArray ($1, $3); }
		| struct_or_union OC struct_members CC
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
			if ($1 == type_struct)
			    $$ = NewTypesStruct (st);
			else
			    $$ = NewTypesUnion (st);
		    }
		| OP type CP
		    { $$ = $2; }
		| typename
		    { $$ = NewTypesName ($1, 0); }
		;
basetype    	: POLY
		| INTEGER
		| RATIONAL
		| REAL
		| STRING
		| FILET
		| MUTEX
		| SEMAPHORE
		| CONTINUATION
		| THREAD
		;
opt_stars	: stars
		|
		    { $$ = 0; }
		;
stars		: stars COMMA TIMES
		    { $$ = NewExprTree (COMMA, 0, $1); }
		| TIMES
		    { $$ = NewExprTree (COMMA, 0, 0); }
		;
struct_or_union	: STRUCT
		    { $$ = type_struct; }
		| UNION
		    { $$ = type_union; }
		;
/*
* Structure member declarations
*/
struct_members	: opt_type names SEMI struct_members
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

publish_extend	: PUBLIC
		| EXTEND
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
		    { $$ = NewArgType ($1.type, False, $1.name, $3); }
		| argdecl opt_dots
		    { $$ = NewArgType ($1.type, $2, $1.name, 0); }
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
		    { $$ = NewArgType ($1.type, False, $1.name, $3); }
		| argdefine opt_dots
		    { $$ = NewArgType ($1.type, $2, $1.name, 0); }
		;
argdefine	: opt_type NAME
		    { $$.type = $1; $$.name = $2; }
		;
opt_dots	: DOTS
		    { $$ = True; }
		|
		    { $$ = False; }
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
		    { $$ = NewExprTree (COMMA, $1, 0); }
		;

/*
* This expression level includes lambdas which can't be in simpleexpr
* because of grammar ambiguities
*/
lambdaexpr	: func_type FUNC OP opt_argdefines CP block
		    { $$ = NewExprCode (NewFuncCode ($1, $4, $6), 0); }
		| simpleexpr
		;
/*
* Fundemental expression production
*/
simpleexpr	: primary
		| MOD const						%prec THREADID
		    {   Value	t;
			t = do_Thread_id_to_thread ($2);
			if (Zerop (t))
			{
			    yyerror ("No thread %v", $2);
			    YYERROR;
			}
			else
			    $$ = NewExprConst(CONST, t); 
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
		| simpleexpr SHIFTL simpleexpr
		    { $$ = NewExprTree(SHIFTL, $1, $3); }
		| simpleexpr SHIFTR simpleexpr
		    { $$ = NewExprTree(SHIFTR, $1, $3); }
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
assignop	: ASSIGNPLUS
		| ASSIGNMINUS
		| ASSIGNTIMES
		| ASSIGNDIVIDE
		| ASSIGNDIV
		| ASSIGNMOD
		| ASSIGNPOW
		| ASSIGNSHIFTL
		| ASSIGNSHIFTR
		| ASSIGNLXOR
		| ASSIGNLAND
		| ASSIGNLOR
		| ASSIGN
		;
initexpr	: primary
		| MOD const						%prec THREADID
		    {   Value	t;
			t = do_Thread_id_to_thread ($2);
			if (Zerop (t))
			{
			    yyerror ("No thread %v", $2);
			    YYERROR;
			}
			else
			    $$ = NewExprConst(CONST, t); 
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
		| initexpr SHIFTL initexpr
		    { $$ = NewExprTree(SHIFTL, $1, $3); }
		| initexpr SHIFTR initexpr
		    { $$ = NewExprTree(SHIFTR, $1, $3); }
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
		    { $$ = NewExprConst(CONST, $1); }
		| CCONST
		    { $$ = NewExprConst(CCONST, $1); }
		| OP type CP inits
		    { 
			$$ = NewExprTree (NEW, $4, 0); 
			$$->base.type = $2; 
		    }
		| OP type CP OS stars CS inits
		    { 
			$$ = NewExprTree (NEW, $7, 0); 
			$$->base.type = NewTypesArray ($2, $5); 
		    }
		| OP type CP OS exprs CS opt_inits
		    { 
			$$ = NewExprTree (NEW, $7, 0); 
			$$->base.type = NewTypesArray ($2, $5); 
		    }
		| OS stars CS inits
		    { 
			$$ = NewExprTree (NEW, $4, 0); 
			$$->base.type = NewTypesArray (typesPoly, $2); 
		    }
		| OS exprs CS opt_inits
		    { 
			$$ = NewExprTree (NEW, $4, 0); 
			$$->base.type = NewTypesArray (typesPoly, $2); 
		    }
		| OP type DOT NAME CP primary			%prec UNIONCAST
		    { 
			$$ = NewExprTree (UNION, NewExprAtom ($4), $6); 
			$$->base.type = $2;
		    }
		| DOLLAR opt_const
		    { $$ = NewExprConst(CONST, History (0, $2)); }
		| DOT
		    { $$ = NewExprConst(CONST, History (0, Zero)); }
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
		| primary COLONCOLON NAME
		    { $$ = NewExprTree(COLONCOLON, $1, NewExprAtom ($3)); }
		| primary ARROW NAME
		    { $$ = NewExprTree(ARROW, $1, NewExprAtom ($3)); }
		;
opt_const	: const
		|
		    { $$ = Zero; }
		;
const		: CONST
		| CCONST
		;
/*
 * Initializers
 */
opt_inits	: inits
		|
		    { $$ = 0; };
		;
inits		: OC arrayinits  CC
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


extern NamespacePtr	CurrentNamespace;
FramePtr	CurrentFrame;

Value
lookupVar (char *name)
{
    ENTER ();
    SymbolPtr	s;
    int		depth;
    Value	v;

    s = NamespaceFindSymbol (CurrentNamespace, AtomId (name), &depth);
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
    s = NamespaceFindSymbol (CurrentNamespace, n, &depth);
    if (!s)
	s = NamespaceAddSymbol (CurrentNamespace, NewSymbolGlobal (n, 0,
							  publish_private));
    if (s->symbol.class == class_global)
	BoxValueSet (s->global.value, 0, v);
    EXIT ();
}

void
GetNamespace (NamespacePtr *scope, FramePtr *fp)
{
    *scope = CurrentNamespace;
    *fp = CurrentFrame;
}

ExprPtr
BuildName (char *name)
{
    ENTER ();
    RETURN (NewExprAtom (AtomId(name)));
}

ExprPtr
BuildCall (char *scope, char *name, int nargs, ...)
{
    ENTER ();
    va_list alist;
    ExprPtr args;
    ExprPtr f;
    ExprPtr e;

    va_start (alist, nargs);
    args = 0;
    while (nargs--)
    {
	args = NewExprTree (COMMA, va_arg (alist, ExprPtr), args);
    }
    va_end (alist);
    f = BuildName (name);
    if (scope)
	f = NewExprTree (COLONCOLON, BuildName (scope), f);
    e = NewExprTree (OP, f, args);
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
    extern Atom	yyfile;
    extern int	yylineno;

    if (yyfile)
	FilePrintf (FileStderr, "%A:%d: ", yyfile, yylineno);
    va_start (args, fmt);
    FileVPrintf (FileStderr, fmt, args);
    FilePrintf (FileStderr, "\n");
    va_end (args);
}
