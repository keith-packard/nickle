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
NamespacePtr	LexNamespace;
int funcDepth;

void yyerror (char *fmt, ...);

static SymbolPtr
ParseNewSymbol (Class class, Types *type, NamePtr name);

%}

%union {
    int		    ints;
    Value	    value;
    Class	    class;
    Type	    type;
    ArgType	    *argType;
    Types	    *types;
    Publish	    publish;
    ExprPtr	    expr;
    Atom	    atom;
    DeclListPtr	    declList;
    MemListPtr	    memList;
    Fulltype	    fulltype;
    ArgDecl	    argDecl;
    NamePtr	    name;
    NamespacePtr    namespace;
    CodePtr	    code;
    Bool	    bool;
    AtomListPtr	    atomList;
}

%type  <expr>	    fullname
%type  <expr>	    opt_rawnames rawname rawnames rawnamespace
%type  <atom>	    rawatom
%type  <expr>	    block func_body statements statement catches catch
%type  <expr>	    case_block cases case
%type  <expr>	    union_case_block union_cases union_case
%type  <expr>	    fulltype
%type  <expr>	    namespace
%type  <atomList>   atoms
%type  <declList>   initnames newtypenames
%type  <name>	    newname newtypename
%type  <expr>	    opt_init
%type  <fulltype>   decl func_decl
%type  <types>	    opt_type type
%type  <expr>	    opt_stars stars
%type  <type>	    basetype
%type  <type>	    struct_or_union
%type  <memList>    struct_members
%type  <class>	    class
%type  <publish>    publish publish_extend
%type  <atom>	    namespacename

%type  <argType>    opt_argdecls argdecls
%type  <argDecl>    argdecl
%type  <argType>    opt_argdefines argdefines
%type  <argDecl>    argdefine
%type  <bool>	    opt_dots

%type  <expr>	    opt_expr expr opt_exprs exprs lambdaexpr simpleexpr primary
%type  <expr>	    comma_expr
%type  <ints>	    assignop
%type  <value>	    opt_integer integer
%type  <expr>	    opt_arrayinit arrayinit arrayelts  arrayelt
%type  <expr>	    structinit structelts structelt
%type  <expr>	    init

%token		    VAR EXPR ARRAY STRUCT UNION

%token		    NL SEMI MOD OC CC DOLLAR DOTS
%token <class>	    GLOBAL AUTO STATIC
%token <type>	    POLY INTEGER NATURAL RATIONAL REAL STRING
%token <type>	    FILET MUTEX SEMAPHORE CONTINUATION THREAD VOID
%token		    FUNCTION FUNC EXCEPTION RAISE
%token		    TYPEDEF IMPORT NEW
%token <namespace>  NAMESPACE
%token <publish>    PUBLIC EXTEND
%token		    IF ELSE WHILE DO FOR SWITCH
%token		    BREAK CONTINUE RETURNTOK FORK CASE DEFAULT
%token		    TRY CATCH TWIXT
%token <atom>	    NAME TYPENAME NAMESPACENAME COMMAND NAMECOMMAND ATOM
%token <value>	    TEN_CONST OCTAL_CONST BINARY_CONST HEX_CONST FLOAT_CONST
%token <value>	    CHAR_CONST STRING_CONST POLY_CONST THREAD_CONST
%token <value>	    VOIDVAL

%nonassoc 	POUND
%right		COMMA
%right <ints>	ASSIGN
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
		    { ignorenl = 0; LexNamespace = 0; funcDepth = 0; }
		;
opt_nl		: NL
		|
		;
opt_semi	: SEMI
		|
		;
opt_comma	: COMMA
		|
		;
/*
 * Interpreter command level
 */
command		: expr attendnl NL
		    {
			ENTER ();
			ExprPtr	e;
			Value	t;
			NamespacePtr	s;
			FramePtr	f;
			
			e = BuildCall ("Command", "display",
				       2,
				       BuildName (0, "format"),
				       BuildCall ("History", "insert",
						  1, NewExprTree (DOLLAR, $1, 0)));
			GetNamespace (&s, &f);
			t = NewThread (f, CompileExpr (e, 0));
			ThreadsRun (t, 0);
			EXIT ();
		    }
		| expr POUND expr attendnl NL
		    {
			ENTER ();
			ExprPtr	e;
			Value	t;
			NamespacePtr	s;
			FramePtr	f;

			e = BuildCall ("File", "print",
				       7,
				       BuildName (0, "stdout"),
				       BuildCall ("History", "insert",
						  1, $1),
				       NewExprConst (STRING_CONST, NewStrString ("g")),
				       $3,
				       NewExprConst (TEN_CONST, Zero),
				       NewExprConst (TEN_CONST, NewInt (-1)),
				       NewExprConst (STRING_CONST, NewStrString (" ")));
			e = NewExprTree (COMMA,
					 e,
					 BuildCall (0, "putchar",
						    1,
						    NewExprConst (CHAR_CONST, NewInt ('\n'))));
			GetNamespace (&s, &f);
			t = NewThread (f, CompileExpr (e, 0));
			ThreadsRun (t, 0);
			EXIT ();
		    }
		| statement attendnl opt_nl
		    { 
			ENTER ();
			NamespacePtr    s;
			FramePtr    f;
			Value	    t;
			
			GetNamespace (&s, &f);
			t = NewThread (f, CompileStat ($1, 0));
			ThreadsRun (t, 0);
			EXIT ();
		    }
		| COMMAND opt_exprs attendnl opt_semi NL
		    {
			ENTER();
			ExprPtr	e;
			Value	t;
			NamespacePtr	s;
			FramePtr	f;
			CommandPtr	c;

			c = CommandFind (CurrentCommands, $1);
			if (!c)
			    yyerror ("Undefined command \"%s\"", AtomName ($1));
			else
			{
			    e = NewExprTree (OP, 
					     NewExprConst (POLY_CONST, c->func),
					     $2);
			    GetNamespace (&s, &f);
			    t = NewThread (f, CompileExpr (e, 0));
			    ThreadsRun (t, 0);
			}
			EXIT ();
		    }
		| NAMECOMMAND opt_rawnames attendnl opt_semi NL
		    {
			ENTER ();
			ExprPtr e;

			Value	t;
			NamespacePtr	s;
			FramePtr	f;
			CommandPtr	c;

			c = CommandFind (CurrentCommands, $1);
			if (!c)
			    yyerror ("Undefined command \"%s\"", AtomName ($1));
			else
			{
			    e = NewExprTree (OP, 
					     NewExprConst (POLY_CONST, c->func),
					     $2);
			    GetNamespace (&s, &f);
			    t = NewThread (f, CompileExpr (e, 0));
			    ThreadsRun (t, 0);
			}
			EXIT ();
		    }
		| NL
		;
opt_rawnames	: rawnames
		|
		    { $$ = 0; }
		;
rawnames	: rawname COMMA rawnames
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| rawname
		    { $$ = NewExprTree (COMMA, $1, 0); }
		;
rawname		: rawnamespace rawatom
		    { $$ = BuildRawname ($1, $2); }
		;
rawatom		: NAME
		| NAMESPACENAME
		| TYPENAME
		;
rawnamespace	: rawnamespace NAMESPACENAME COLONCOLON
		    { $$ = NewExprTree (COLONCOLON, $1, NewExprAtom ($2)); }
                |
		    { $$ = 0; }
                ;
fulltype	: namespace TYPENAME
		    { 
			$$ = BuildFullname ($1, $2);
			LexNamespace = 0;
		    }
		| TYPENAME
		    { 
			$$ = BuildFullname (0, $1);
			LexNamespace = 0;
		    }
		;
fullname	: namespace namespacename
		    { 
			$$ = BuildFullname ($1, $2);
			LexNamespace = 0;
		    }
		| namespacename
		    {
			$$ = BuildFullname (0, $1);
			LexNamespace = 0;
		    }
		;
namespace	: namespace NAMESPACENAME COLONCOLON
		    { 
			ExprPtr	    e;
			NamePtr	name;

			e = BuildFullname ($1, $2);
			if (e->base.tag == COLONCOLON)
			    name = e->tree.right->name.name;
			else
			    name = e->name.name;
			if (!name || !NameSymbol (name))
			{
			    yyerror ("non-existant namespace \"%A\"", $2);
			    YYERROR;
			}
			else if (NameSymbol (name)->symbol.class != class_namespace)
			{
			    yyerror ("%A is not a namespace", $2);
			    YYERROR;
			}
			LexNamespace = NameSymbol (name)->namespace.namespace;
			$$ = e;
		    }
		| NAMESPACENAME COLONCOLON
		    { 
			ExprPtr	    e;
			NamePtr	name;

			e = BuildFullname (0, $1);
			if (e->base.tag == COLONCOLON)
			    name = e->tree.right->name.name;
			else
			    name = e->name.name;
			if (!name || !NameSymbol (name))
			{
			    yyerror ("non-existant namespace \"%A\"", $1);
			    YYERROR;
			}
			else if (NameSymbol (name)->symbol.class != class_namespace)
			{
			    yyerror ("%A is not a namespace", $1);
			    YYERROR;
			}
			LexNamespace = NameSymbol (name)->namespace.namespace;
			$$ = e;
		    }
		;
namespacename	:   NAME
		|   NAMESPACENAME
		;
/*
* Statements
*/
namespace_start	:
		    { CurrentNamespace = NewNamespace (CurrentNamespace); }
		;
namespace_end	:
		    { CurrentNamespace = CurrentNamespace->previous; }
		;
block		: block_start ignorenl statements block_end
		    { $$ = $3; }
		;
block_start	: OC namespace_start
		;
block_end	: namespace_end CC
		;
statements	: statement statements
		    { $$ = NewExprTree(OC, $1, $2); }
		|
		    { $$ = NewExprTree(OC, 0, 0); }
		;
statement	: IF ignorenl namespace_start OP expr CP statement namespace_end
		    { $$ = NewExprTree(IF, $5, $7); }
		| IF ignorenl namespace_start OP expr CP statement ELSE statement namespace_end
		    { $$ = NewExprTree(ELSE, $5, NewExprTree(ELSE, $7, $9)); }
		| WHILE ignorenl namespace_start OP expr CP statement namespace_end
		    { $$ = NewExprTree(WHILE, $5, $7); }
		| DO ignorenl namespace_start statement WHILE OP expr CP namespace_end
		    { $$ = NewExprTree(DO, $4, $7); }
		| FOR ignorenl namespace_start OP opt_expr SEMI opt_expr SEMI opt_expr CP statement namespace_end
		    { $$ = NewExprTree(FOR, NewExprTree(FOR, $5, $7),
				       NewExprTree(FOR, $9, $11));
		    }
		| SWITCH ignorenl namespace_start OP expr CP case_block namespace_end
		    { $$ = NewExprTree (SWITCH, $5, $7); }
		| UNION SWITCH ignorenl namespace_start OP expr CP union_case_block namespace_end
		    { $$ = NewExprTree (UNION, $6, $8); }
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
		| func_decl FUNCTION ignorenl newname namespace_start OP opt_argdefines CP func_body namespace_end
		    { 
			ExprPtr	decl;
			Types	*type = NewTypesFunc ($1.type, $7);
			ExprPtr	nameExpr;

			$4->symbol = ParseNewSymbol ($1.class, type, $4);
			$4->publish = $1.publish;

			decl = NewExprDecl (NewDeclList ($4, 0, 0),
					    $1.class,
					    type,
					    $1.publish);
			if ($9)
			{
			    nameExpr = NewExprName ($4);
			    $$ = NewExprTree (FUNCTION, decl,
					      NewExprTree (ASSIGN,
							   nameExpr,
							   NewExprCode (NewFuncCode ($1.type,
										     $7,
										     $9),
									nameExpr)));
			}
			else
			    $$ = decl;
		    }
		| publish EXCEPTION ignorenl newname namespace_start opt_argdecls namespace_end SEMI
		    { 
			$4->symbol = ParseNewSymbol (class_exception, typesPoly, $4);
			$4->publish = $1;

			$$ = NewExprDecl (NewDeclList ($4, 0, 0),
					class_exception,
					NewTypesFunc (typesPoly, $6),
					$1);
		    }
		| RAISE fullname OP opt_exprs CP SEMI;
		    { $$ = NewExprTree (RAISE, $2, $4); }
		| publish TYPEDEF ignorenl newtypenames SEMI
		    { 
			DeclListPtr decl;

			for (decl = $4; decl; decl = decl->next)
			{
			    decl->name->symbol = NewSymbolType (decl->name->atom, 0);
			    decl->name->publish = $1;
			}
			$$ = NewExprTree (TYPEDEF, NewExprDecl ($4, class_typedef, 0, $1), 0);
		    }
		| publish TYPEDEF ignorenl type newtypenames SEMI
		    { 
			DeclListPtr decl;

			for (decl = $5; decl; decl = decl->next)
			{
			    if (NameSymbol (decl->name))
			    {
				if (NameSymbol (decl->name)->symbol.type)
				{
				    yyerror ("redefinition of typedef %A", decl->name->atom);
				    YYERROR;
				}
				NameSymbol (decl->name)->symbol.type = $4;
			    }
			    else
				decl->name->symbol = NewSymbolType (decl->name->atom, $4);
			    decl->name->publish = $1;
			}
			$$ = NewExprTree (TYPEDEF, NewExprDecl ($5, class_typedef, $4, $1), 0);
		    }
		| publish_extend NAMESPACE ignorenl namespacename
		    {
			NamePtr	    name = 0;
			Publish	    publish = $1;
			
			/*
			 * this is a hack - save the current namespace
			 * on the parser stack to be restored after
			 * the block is compiled.
			 */
			$2 = CurrentNamespace;
			if (publish == publish_extend)
			{
			    name = NamespaceFindName (CurrentNamespace, $4, True);
			    if (!name || !NameSymbol (name))
			    {
				yyerror ("non-existant namespace %A", $4);
				YYERROR;
			    }
			    else if (NameSymbol (name)->symbol.class != class_namespace)
			    {
				yyerror ("%A is not a namespace", $4);
				YYERROR;
			    }
			    else
				CurrentNamespace = NameSymbol (name)->namespace.namespace;
			}
			else
			{
			    name = NamespaceNewName (CurrentNamespace, $4);
			    CurrentNamespace = NewNamespace (CurrentNamespace);
			    name->symbol = NewSymbolNamespace ($4);
			    name->symbol->namespace.namespace = CurrentNamespace;
			    name->publish = publish;
			}
			/*
			 * Make all of the symbols visible while compiling within
			 * the namespace
			 */
			if (CurrentNamespace != $2)
			    CurrentNamespace->publish = publish_public;
		    }
			OC statements CC
		    {
			/*
			 * close the namespace to non-public lookups
			 */
			if (CurrentNamespace != $2)
			    CurrentNamespace->publish = publish_private;
			/*
			 * Restore to the namespace saved on
			 * the parser stack
			 */
			CurrentNamespace = $2;
			$$ = NewExprTree (NAMESPACE, 
					  NewExprName (NamespaceFindName (CurrentNamespace, 
									  $4, True)),
					  $7);
		    }
		| publish IMPORT ignorenl fullname SEMI
		    {
			NamePtr	name;

			if ($4->base.tag == COLONCOLON)
			    name = $4->tree.right->name.name;
			else
			    name = $4->name.name;
			if (!name || !NameSymbol (name))
			{
			    yyerror ("non-existant namespace %A", $4);
			    YYERROR;
			}
			else if (NameSymbol (name)->symbol.class != class_namespace)
			{
			    yyerror ("%A is not a namespace", $4);
			    YYERROR;
			}
			NamespaceImport (CurrentNamespace, 
					 NameSymbol (name)->namespace.namespace, $1);
			$$ = NewExprTree (IMPORT, $4, 0);
		    }
		| TRY ignorenl statement catches
		    { $$ = NewExprTree (CATCH, $3, $4); }
		| TWIXT ignorenl namespace_start OP opt_expr SEMI opt_expr CP statement namespace_end
		    { $$ = NewExprTree (TWIXT, 
					NewExprTree (TWIXT, $5, $7),
					NewExprTree (TWIXT, $9, 0));
		    }
		| TWIXT ignorenl namespace_start OP opt_expr SEMI opt_expr CP statement ELSE statement namespace_end
		    { $$ = NewExprTree (TWIXT, 
					NewExprTree (TWIXT, $5, $7),
					NewExprTree (TWIXT, $9, $11));
		    }
		;
catches		:   catch catches
		    { $$ = NewExprTree (CATCH, $1, $2); }
		|   
		    { $$ = 0; }
		;
catch		: CATCH fullname namespace_start OP opt_argdefines CP block namespace_end
		    { $$ = NewExprCode (NewFuncCode (typesPoly, $5, $7), $2); }
func_body    	: { ++funcDepth; } block { --funcDepth; }
		    { $$ = $2; }
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
atoms		: NAME COMMA atoms
		    { $$ = NewAtomList ($3, $1); }
		| NAME
		    { $$ = NewAtomList (0, $1); }
		;
newname		: NAME
		    { $$ = NamespaceNewName (CurrentNamespace, $1); }
		;
newtypenames	: newtypename COMMA newtypenames
		    { $$ = NewDeclList ($1, 0, $3); }
		| newtypename
		    { $$ = NewDeclList ($1, 0, 0); }
		;
newtypename	: TYPENAME
		    { 
			NamePtr	name;

			name = NamespaceFindName (CurrentNamespace, $1, False);
			if (!name || 
			    (NameSymbol (name) && NameSymbol (name)->symbol.type))
			    name = NamespaceNewName (CurrentNamespace, $1);
			$$ = name;
		    }
		| NAME
		    { 
			NamePtr	name;

			name = NamespaceFindName (CurrentNamespace, $1, False);
			if (!name || NameSymbol (name))
			    name = NamespaceNewName (CurrentNamespace, $1); 
			$$ = name;
		    }
		;
initnames	: newname opt_init COMMA initnames
		    { $$ = NewDeclList ($1, $2, $4); }
		| newname opt_init
		    { $$ = NewDeclList ($1, $2, 0); }
		;
opt_init	: ASSIGN simpleexpr
		    { $$ = $2; }
		|
		    { $$ = 0; }
		;
/*
* Full declaration including storage, type and publication
*/
func_decl	: decl
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
		| PUBLIC class
		    { $$.publish = $1; $$.class = $2; $$.type = typesPoly; }
		| class
		    { $$.publish = publish_private; $$.class = $1; $$.type = typesPoly; }
		| PUBLIC
		    { $$.publish = $1; $$.class = class_undef; $$.type = typesPoly; }
		;
/*
* Type declarations
*/
opt_type	: type
		|
		    { $$ = typesPoly; }
		;
type		: basetype
		    {
			if ($1 == type_undef) 
			    $$ = typesPoly;
			else
			    $$ = typesPrim[$1]; 
		    }
		| TIMES type			%prec STAR
		    { $$ = NewTypesRef ($2); }
		| type opt_argdecls	%prec CALL
		    { $$ = NewTypesFunc ($1, $2); }
		| type OS opt_stars CS
		    { $$ = NewTypesArray ($1, $3); }
		| struct_or_union OC struct_members CC
		    {
			AtomListPtr	al;
			StructType	*st;
			StructElement	*se;
			MemListPtr	ml;
			int		nelements;

			nelements = 0;
			for (ml = $3; ml; ml = ml->next)
			{
			    for (al = ml->atoms; al; al = al->next)
				nelements++;
			}
			st = NewStructType (nelements);
			se = StructTypeElements (st);
			nelements = 0;
			for (ml = $3; ml; ml = ml->next)
			{
			    for (al = ml->atoms; al; al = al->next)
			    {
				se[nelements].type = ml->type;
				se[nelements].name = al->id;
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
		| fulltype
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
		| VOID
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
struct_members	: opt_type atoms SEMI struct_members
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
opt_argdecls	: OP argdecls CP
		    { $$ = $2; }
		| OP CP
		    { $$ = 0; }
		;
argdecls	: argdecl COMMA argdecls
		    { $$ = NewArgType ($1.type, False, $1.name, $3); }
		| argdecl opt_dots
		    { $$ = NewArgType ($1.type, $2, $1.name, 0); }
argdecl		: type newname
		    { 
			ParseCanonType ($1);
			$$.type = $1; 
			$$.name = $2; 
		    }
		| type
		    { 
			ParseCanonType ($1);
			$$.type = $1;
			$$.name = 0; 
		    }
		| newname
		    { $$.type = typesPoly; $$.name = $1; }
		;

/*
* Arguments in function definitions
*/
opt_argdefines	: argdefines
		    {
			ArgType	*args;
			Types	*type;

			for (args = $1; args; args = args->next)
			{
			    type = args->type;
			    if (!ParseCanonType (type))
				break;
			    if (args->varargs)
			    {
				type = NewTypesArray (type,
						      NewExprTree (COMMA,
								   NewExprConst (TEN_CONST, 
										 NewInt (0)),
								   0));
			    }
			    args->name->symbol = ParseNewSymbol (class_arg, type, args->name);
			    args->name->publish = publish_private;
			}
		    }
		|
		    { $$ = 0; }
		;
argdefines	: argdefine COMMA argdefines
		    { $$ = NewArgType ($1.type, False, $1.name, $3); }
		| argdefine opt_dots
		    { $$ = NewArgType ($1.type, $2, $1.name, 0); }
		;
argdefine	: opt_type newname
		    { $$.type = $1; $$.name = $2; }
		;
opt_dots	: DOTS
		    { $$ = True; }
		|
		    { $$ = False; }
		;

/*
* Expressions, top level includes comma operator and declarations
*/
opt_expr	: expr
		|
		    { $$ = 0; }
		;
expr		: comma_expr
		| decl initnames
		    { 
			DeclList    *decl;

			for (decl = $2; decl; decl = decl->next)
			{
			    decl->name->symbol = ParseNewSymbol ($1.class, $1.type, decl->name);
			    decl->name->publish = $1.publish;
			}

			$$ = NewExprDecl ($2, $1.class, $1.type, $1.publish); 
		    }
		;
comma_expr	: lambdaexpr
		| comma_expr COMMA lambdaexpr
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
lambdaexpr	: opt_type FUNC namespace_start OP opt_argdefines CP block namespace_end
		    { $$ = NewExprCode (NewFuncCode ($1, $5, $7), 0); }
		| simpleexpr
		;
/*
* Fundemental expression production
*/
simpleexpr	: primary
		| MOD integer						%prec THREADID
		    {   Value	t;
			t = do_Thread_id_to_thread ($2);
			if (Zerop (t))
			{
			    yyerror ("No thread %v", $2);
			    YYERROR;
			}
			else
			    $$ = NewExprConst(THREAD_CONST, t); 
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
		    { 
			$$ = NewExprTree(POW, 
					 BuildName ("Math", "pow"), 
					 NewExprTree (POW, $1, $3)); 
		    }
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
		    { 
			if ($2 == ASSIGNPOW)
			    $$ = NewExprTree (ASSIGNPOW, 
					      BuildName ("Math", "assign_pow"),
					      NewExprTree (ASSIGNPOW, $1, $3));
			else
			{
			    /*
			     * Automatically declare names used in
			     * simple assignements at the top level
			     */
			    if ($2 == ASSIGN && 
				funcDepth == 0 &&
				$1->base.tag == NAME && 
				!(NameSymbol ($1->name.name)))
			    {
				NamePtr	name = $1->name.name;
				name->symbol = ParseNewSymbol (class_undef, typesPoly, name);
				name->publish = publish_private;
			    }
			    $$ = NewExprTree($2, $1, $3); 
			}
		    }
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
primary		: fullname
		| TEN_CONST
		    { $$ = NewExprConst(TEN_CONST, $1); }
		| OCTAL_CONST
		    { $$ = NewExprConst(OCTAL_CONST, $1); }
		| BINARY_CONST
		    { $$ = NewExprConst(BINARY_CONST, $1); }
		| HEX_CONST
		    { $$ = NewExprConst(HEX_CONST, $1); }
		| FLOAT_CONST
		    { $$ = NewExprConst(FLOAT_CONST, $1); }
		| CHAR_CONST
		    { $$ = NewExprConst(CHAR_CONST, $1); }
		| STRING_CONST
		    { $$ = NewExprConst(STRING_CONST, $1); }
		| VOIDVAL
		    { $$ = NewExprConst(VOIDVAL, $1); }
		| OP type CP ignorenl namespace_start init namespace_end
		    { 
			ParseCanonType ($2);
			$$ = NewExprTree (NEW, $6, 0); 
			$$->base.type = $2;
		    }
		| OP type OS exprs CS CP ignorenl namespace_start opt_arrayinit namespace_end
		    { 
			ParseCanonType ($2);
			$$ = NewExprTree (NEW, $9, 0); 
			$$->base.type = NewTypesArray ($2, $4); 
		    }
		| OS stars CS ignorenl namespace_start arrayinit namespace_end
		    { 
			$$ = NewExprTree (NEW, $6, 0); 
			$$->base.type = NewTypesArray (typesPoly, $2); 
			ParseCanonType ($$->base.type);
		    }
		| OS exprs CS ignorenl namespace_start opt_arrayinit namespace_end
		    { 
			$$ = NewExprTree (NEW, $6, 0); 
			$$->base.type = NewTypesArray (typesPoly, $2); 
		    }
		| OP type DOT NAME CP primary			%prec UNIONCAST
		    {
			ParseCanonType ($2);
			$$ = NewExprTree (UNION, NewExprAtom ($4), $6); 
			$$->base.type = $2;
		    }
		| DOLLAR opt_integer
		    { $$ = NewExprConst(POLY_CONST, History (0, $2)); }
		| DOT
		    { $$ = NewExprConst(POLY_CONST, History (0, Zero)); }
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
opt_integer	: integer
		|
		    { $$ = Zero; }
		;
integer		: TEN_CONST
		| OCTAL_CONST
		| BINARY_CONST
		| HEX_CONST
		;
/*
 * Array initializers
 */
opt_arrayinit	: arrayinit
		| OC CC
		    { $$ = 0; }
		|
		    { $$ = 0; }
		;
arrayinit    	: OC arrayelts opt_comma opt_dots CC
		    { 
			ExprPtr	elts = ExprRehang ($2, 0);
			if ($4)
			{
			    ExprPtr i = elts;
			    while (i->tree.right)
				i = i->tree.right;
			    i->tree.right = NewExprTree (COMMA, 
							 NewExprTree (DOTS, 0, 0),
							 0);
			}
			$$ = NewExprTree (ARRAY, elts, 0); 
		    }
		;
arrayelts	: arrayelts COMMA arrayelt
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| arrayelt
		    { $$ = NewExprTree (COMMA, 0, $1); }
		;
arrayelt	: lambdaexpr
		| arrayinit
		;

/* 
* Structure initializers
*/
structinit    	: OC structelts opt_comma CC
		    { $$ = NewExprTree (STRUCT, ExprRehang ($2, 0), 0); }
		| OC CC
		    { $$ = NewExprTree (STRUCT, 0, 0); }
		;
structelts	: structelts COMMA structelt
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| structelt
		    { $$ = NewExprTree (COMMA, 0, $1); }
		;
structelt	: NAME ASSIGN lambdaexpr
		    { $$ = NewExprTree (ASSIGN, NewExprAtom ($1), $3); }
		;

init		: arrayinit
		| structinit
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
NewDeclList (NamePtr name, ExprPtr init, DeclListPtr next)
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
    MemReference (ml->atoms);
}

DataType MemListType = { MemListMark, 0 };

MemListPtr
NewMemList (AtomListPtr atoms, Types *type, MemListPtr next)
{
    ENTER ();
    MemListPtr	ml;

    ml = ALLOCATE (&MemListType, sizeof (MemList));
    ml->type = type;
    ml->atoms = atoms;
    ml->next = next;
    RETURN (ml);
}


extern NamespacePtr	CurrentNamespace;
FramePtr	CurrentFrame;

Value
lookupVar (char *n)
{
    ENTER ();
    Value	v;
    NamePtr	name;

    name = NamespaceFindName (CurrentNamespace, AtomId (n), True);
    if (name && NameSymbol (name) && NameSymbol (name)->symbol.class == class_global)
	v = BoxValue (NameSymbol (name)->global.value, 0);
    else
	v = Zero;
    RETURN (v);
}

void
setVar (NamespacePtr namespace, char *n, Value v, Types *type)
{
    ENTER ();
    Atom	atom;
    SymbolPtr	s;
    NamePtr	name;

    atom = AtomId (n);
    name = NamespaceFindName (namespace, atom, True);
    if (!name)
	name = NamespaceNewName (namespace, atom);
    if (!NameSymbol (name))
    {
	name->symbol = NewSymbolGlobal (atom, type);
	name->publish = publish_private;
    }
    s = name->symbol;
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
BuildName (char *ns, char *n)
{
    ENTER ();
    NamePtr	    name, ns_name = 0;
    Bool	    search = True;
    NamespacePtr    namespace = CurrentNamespace;
    ExprPtr	    e;

    if (ns)
    {
	ns_name = NamespaceFindName (namespace, AtomId (ns), search);
	if (ns_name && NameSymbol (ns_name) && NameSymbol (ns_name)->symbol.class == class_namespace)
	    namespace = NameSymbol (ns_name)->namespace.namespace;
	else
	    namespace = 0;
	search = False;
    }
    name = 0;
    if (namespace)
	name = NamespaceFindName (namespace, AtomId (n), search);
    if (!name)
	name = NewName (0, AtomId (n));
    e = NewExprName (name);
    if (ns_name)
	e = NewExprTree (COLONCOLON, NewExprName (ns_name), e);
    RETURN (e);
}

static Value
AtomString (Atom id)
{
    ENTER ();
    RETURN (NewStrString (AtomName (id)));
}

ExprPtr
BuildRawname (ExprPtr colonnames, Atom name)
{
    ENTER ();
    int     len;
    Value   array;
    ExprPtr e;

    len = 1;
    for (e = colonnames; e; e = e->tree.left)
	len++;
    array = NewArray (False, typesPrim[type_string], 1, &len);
    len--;
    BoxValueSet (array->array.values, len, AtomString (name));
    e = colonnames;
    while (--len >= 0)
    {
	BoxValueSet (array->array.values, len, AtomString (e->tree.right->atom.atom));
	e = e->tree.left;
    }
    e = NewExprConst (POLY_CONST, array);
    RETURN (e);
}

	    
ExprPtr
BuildFullname (ExprPtr left, Atom atom)
{
    ENTER ();
    NamespacePtr    namespace;
    NamePtr	    name;
    SymbolPtr	    sym;
    Bool	    search;
    ExprPtr	    nameExpr;

    if (left)
    {
	if (left->base.tag == COLONCOLON)
	    sym = NameSymbol (left->tree.right->name.name);
	else
	    sym = NameSymbol (left->name.name);
    }
    else
	sym = 0;
    if (sym && sym->symbol.class == class_namespace)
    {
	namespace = sym->namespace.namespace;
	search = False;
    }
    else
    {
	namespace = CurrentNamespace;
	search = True;
    }
    name = NamespaceFindName (namespace, atom, search);
    if (!name)
	name = NamespaceNewName (namespace, atom);
    nameExpr = NewExprName (name);
    if (left)
	nameExpr = NewExprTree (COLONCOLON, left, nameExpr);
    RETURN (nameExpr);
}

ExprPtr
BuildCall (char *scope, char *name, int nargs, ...)
{
    ENTER ();
    va_list	    alist;
    ExprPtr	    args;
    ExprPtr	    f;
    ExprPtr	    e;

    va_start (alist, nargs);
    args = 0;
    while (nargs--)
    {
	args = NewExprTree (COMMA, va_arg (alist, ExprPtr), args);
    }
    va_end (alist);
    f = BuildName (scope, name);
    e = NewExprTree (OP, f, args);
#ifdef DEBUG
    printExpr (stdout, e, -1, 0);
    printf ("\n");
#endif
    RETURN (e);
}

/*
 * Create a new symbol in the specified namespace, as a special
 * case, re-use symbols if not inside a function, but require
 * that the types match exactly
 */
static Bool
ParseCanonType (TypesPtr type)
{
    NamePtr	name;
    SymbolPtr	s;
    ArgType	*arg;
    int		n;
    Bool	ret = True;
    
    if (!type)
    {
	yyerror ("Type missing inside compiler");
	return False;
    }
    switch (type->base.tag) {
    case types_prim:
	break;
    case types_name:
	if (!type->name.type)
	{
	    name = TypeNameName (type);
	    s = NameSymbol (name);
	    if (!s)
	    {
		yyerror ("No typedef \"%A\" in namespace",
			      name->atom);
		ret = False;
	    }
	    else if (s->symbol.class != class_typedef)
	    {
		yyerror ("Symbol \"%A\" not a typedef",
			      name->atom);
		ret = False;
	    }
	    else if (!s->symbol.type)
	    {
		yyerror ("Typedef \"%A\" not defined yet",
			      name->atom);
		ret = False;
	    }
	    else
	    {
		type->name.type = s->symbol.type;
		ret = ParseCanonType (type->name.type);
	    }
	}
	break;
    case types_ref:
    	ret = ParseCanonType (type->ref.ref);
	break;
    case types_func:
	if (type->func.ret)
	    ret = ParseCanonType (type->func.ret);
	for (arg = type->func.args; arg; arg = arg->next)
	    ret = ret && ParseCanonType (arg->type);
	break;
    case types_array:
	ret = ParseCanonType (type->array.type);
	break;
    case types_struct:
    case types_union:
	for (n = 0; n < type->structs.structs->nelements; n++)
	{
	    StructElement   *se;

	    se = &StructTypeElements(type->structs.structs)[n];
	    ret = ret && ParseCanonType (se->type);
	}
	break;
    }
    return ret;
}

static SymbolPtr
ParseNewSymbol (Class class, Types *type, NamePtr name)
{
    ENTER ();
    SymbolPtr	s = 0;
    
    if (class == class_undef)
	class = funcDepth ? class_auto : class_global;

    if (ParseCanonType (type))
    {
	switch (class) {
	case class_global:
	    s = NewSymbolGlobal (name->atom, type);
	    break;
	case class_static:
	    s = NewSymbolStatic (name->atom, type);
	    break;
	case class_arg:
	    s = NewSymbolArg (name->atom, type);
	    break;
	case class_auto:
	    s = NewSymbolAuto (name->atom, type);
	    break;
	case class_exception:
	    s = NewSymbolException (name->atom, type);
	    break;
	default:
	    break;
	}
    }
    RETURN (s);
}

int
yywrap (void)
{
    if (LexInteractive())
	printf ("\n");
    return 1;
}

void
yyerror (char *fmt, ...)
{
    va_list	args;

    if (LexFileName ())
	FilePrintf (FileStderr, "%A:%d: ", LexFileName (), LexFileLine ());
    va_start (args, fmt);
    FileVPrintf (FileStderr, fmt, args);
    FilePrintf (FileStderr, "\n");
    va_end (args);
}
