%{

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	<stdio.h>

/*
 * This grammar generates 1 shift/reduce conflict and 4 reduce/reduce:
 * 
 * 2 reduce/reduce conflicts on:
 *
 *	    *int . func 
 *	    **int . func
 *
 *	Is the '*' a dereference operator or a pointer type modifier?
 *	The grammar is ordered to make this a type modifier since
 *	the other way is less common.  Use parens when you want this.
 *
 * 2 reduce/reduce conflicts on:
 *
 *	   &int . func
 *	   &&int . func
 *
 *	Is the '&' a reference operator or a reference type modifier?
 *	The grammar is ordered to make this a type modifier since
 *	the other way is less common.  Use parens when you want this.
 *
 *  shift/reduce conflict in ASSIGN in initializers
 *
 *	That's because struct initializers look like assignment
 *	expressions while array initializers permit any expression.
 *	Shift yields struct initialization, so the effect is
 *	to make assignment expressions invalid in array initializers.
 *	(Of course, you can always enclose an assignment in parens.)
 *      (I can't see any obvious way to use operator precedence to
 *      get rid of this: seems like you'd need to duplicate simpleexpr
 *      with different precedence. --BCM)
 *
 */
int ignorenl;
int notCommand;
int seeComment;
NamespacePtr	LexNamespace;
int funcDepth;

void ParseError (char *fmt, ...);
void yyerror (char *msg);

typedef enum _CanonTypeResult {
    CanonTypeUndefined,
    CanonTypeForward,
    CanonTypeDefined,
} CanonTypeResult;
    
static Expr *
ParseCanonFor (Expr *expr);

static CanonTypeResult
ParseCanonType (TypePtr type, Bool forwardAllowed);

static SymbolPtr
ParseNewSymbol (Publish publish, Class class, Type *type, Atom name);

%}

%union {
    int		    ints;
    Value	    value;
    Class	    class;
    ArgType	    *argType;
    Type	    *type;
    Publish	    publish;
    ExprPtr	    expr;
    Atom	    atom;
    DeclListPtr	    declList;
    MemListPtr	    memList;
    Fulltype	    fulltype;
    ArgDecl	    argDecl;
    SymbolPtr	    symbol;
    NamespacePtr    namespace;
    CodePtr	    code;
    Bool	    bool;
    AtomListPtr	    atomList;
    FuncDecl	    funcDecl;
}

%type  <expr>	    fullname fullnames
%type  <expr>	    opt_rawnames rawname rawnames rawnamespace
%type  <atom>	    rawatom
%type  <expr>	    block opt_func_body func_body func_right catches catch
%type  <expr>	    statements statement simple_statement
%type  <expr>	    if_statement try_statement try_body_statement
%type  <expr>	    block_or_expr
%type  <expr>	    case_block cases case
%type  <expr>	    union_case_block union_cases union_case
%type  <expr>	    fulltype
%type  <expr>	    namespace
%type  <expr>	    comprehension compnames compname comparray
%type  <atomList>   atoms
%type  <declList>   initnames typenames
%type  <symbol>	    name opt_name
%type  <funcDecl>   func_decl func_name
%type  <atom>	    typename 
%type  <expr>	    opt_init
%type  <fulltype>   decl next_decl
%type  <type>	    opt_type type subscripts subtype
%type  <expr>	    opt_stars stars dotdotdots
%type  <type>	    basetype
%type  <expr>	    dims
%type  <memList>    struct_members union_members
%type  <class>	    class opt_class
%type  <publish>    opt_publish publish publish_extend
%type  <atom>	    namespacename

%type  <argType>    opt_argdecls argdecls
%type  <argDecl>    argdecl
%type  <argType>    opt_argdefines argdefines args
%type  <argDecl>    argdefine
%type  <bool>	    opt_dotdotdot

%type  <expr>	    opt_expr for_exprs expr opt_exprs exprs simpleexpr
%type  <expr>	    opt_actuals actuals
%type  <expr>	    comma_expr
%type  <ints>	    assignop
%type  <value>	    opt_integer integer
%type  <value>	    doc_string opt_comment
%type  <expr>	    opt_arrayinit arrayinit arrayelts  arrayelt
%type  <expr>	    opt_hashinit hashinit hashelts hashelt hashvalue
%type  <expr>	    structinit structelts structelt
%type  <expr>	    init

%token		    VAR EXPR ARRAY STRUCT UNION ENUM COMP HASH

%token		    SEMI MOD OC CC DOLLAR DOTDOTDOT ENDFILE
%token <class>	    GLOBAL AUTO STATIC CONST
%token <type>	    POLY INTEGER NATURAL RATIONAL REAL STRING FOREIGN
%token <type>	    FILET MUTEX SEMAPHORE CONTINUATION THREAD VOID BOOL
%token		    FUNCTION FUNC EXCEPTION RAISE
%token		    TYPEDEF IMPORT NEW ANONINIT
%token <namespace>  NAMESPACE
%token <publish>    PUBLIC PROTECTED EXTEND
%token		    WHILE DO FOR SWITCH
%token		    BREAK CONTINUE RETURNTOK FORK CASE DEFAULT
%token		    TWIXT
%token <atom>	    NAME TYPENAME NAMESPACENAME COMMAND NAMECOMMAND
%token <value>	    TEN_NUM OCTAL0_NUM OCTAL_NUM BINARY_NUM HEX_NUM
%token <value>	    TEN_FLOAT OCTAL0_FLOAT OCTAL_FLOAT BINARY_FLOAT HEX_FLOAT
%token <value>	    CHAR_CONST STRING_CONST POLY_CONST THREAD_CONST
%token <value>	    COMMENT_CONST
%token <value>	    VOIDVAL BOOLVAL
%token		    DARROW
%token		    ISTYPE HASMEMBER

%nonassoc 	POUND
%right		COMMA
%right <ints>	ASSIGN
		ASSIGNPLUS ASSIGNMINUS ASSIGNTIMES ASSIGNDIVIDE 
		ASSIGNDIV ASSIGNMOD ASSIGNPOW
		ASSIGNSHIFTL ASSIGNSHIFTR
		ASSIGNLXOR ASSIGNLAND ASSIGNLOR
                ASSIGNOR ASSIGNAND
%right		FORK
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
%right		POW STARSTAR POW2 POW3
%left		UNIONCAST
%right		UMINUS BANG FACT LNOT INC DEC STAR AMPER THREADID
%left		OS CS DOT ARROW STAROS CALL OP CP
%right		POINTER
%right		COLONCOLON

%left IF TRY NONL
%left ELSE CATCH NL

%%
lines		: lines pcommand
		|
		;
pcommand	: command
		| error reset eend
		;
eend		: NL
		    { yyerrok; }
		| SEMI
		    { yyerrok; }
		| ENDFILE
		    { yyerrok; }
		;

/*
 * Sprinkled through the grammer to switch the lexer between reporting
 * and not reporting newlines
 */
ignorenl	:
		    { ignorenl++; }
		;
attendnl	:
		    { if (ignorenl > 0) ignorenl--; }
		;
reset		:
		    { 
			ignorenl = 0; 
			LexNamespace = 0; 
			CurrentNamespace = TopNamespace; 
			funcDepth = 0; 
			notCommand = 0;
			lastThreadError = True;
		    }

		;

/*
 * To make commands only recognized as the first token on a line,
 * this production precedes every top-level production in the grammar.
 * As the parser will look one token ahead, this cleverly makes only
 * the first token on the line match commands
 */
not_command	:
		    { notCommand = 1; }
		;
opt_nl		: NL
		|  %prec NONL
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
command		: not_command expr reset NL
		    {
			ENTER ();
			ExprPtr	e;
			Value	t;
			NamespacePtr	s;
			FramePtr	f;
			CodePtr		c;
			
			e = BuildCall ("Command", "display",
				       1,NewExprTree (EXPR, $2, 0));
			GetNamespace (&s, &f, &c);
			t = NewThread (f, CompileExpr (e, c));
			ThreadsRun (t, 0);
			EXIT ();
		    }
		| not_command expr POUND expr reset NL
		    {
			ENTER ();
			ExprPtr	e;
			Value	t;
			NamespacePtr	s;
			FramePtr	f;
			CodePtr		c;

			e = BuildCall("Command", "display_base",
				      2, NewExprTree (EXPR, $2, 0), $4);
			GetNamespace (&s, &f, &c);
			t = NewThread (f, CompileExpr (e, c));
			ThreadsRun (t, 0);
			EXIT ();
		    }
		| not_command statement reset opt_nl
		    { 
			ENTER ();
			NamespacePtr    s;
			FramePtr	f;
			CodePtr		c;
			Value		t;
			
			GetNamespace (&s, &f, &c);
			t = NewThread (f, CompileStat ($2, c));
			ThreadsRun (t, 0);
			EXIT ();
		    }
		| not_command COMMAND opt_exprs reset opt_semi NL
		    {
			ENTER();
			ExprPtr	e;
			Value	t;
			NamespacePtr	s;
			FramePtr	f;
			CodePtr		c;
			CommandPtr	cmd;

			cmd = CommandFind (CurrentCommands, $2);
			if (!cmd)
			    ParseError ("Undefined command \"%s\"", AtomName ($2));
			else
			{
			    e = NewExprTree (OP, 
					     NewExprConst (POLY_CONST, cmd->func),
					     $3);
			    GetNamespace (&s, &f, &c);
			    t = NewThread (f, CompileExpr (e, c));
			    ThreadsRun (t, 0);
			}
			EXIT ();
		    }
		| not_command NAMECOMMAND opt_rawnames reset opt_semi NL
		    {
			ENTER ();
			ExprPtr e;

			Value	t;
			NamespacePtr	s;
			FramePtr	f;
			CodePtr		c;
			CommandPtr	cmd;

			cmd = CommandFind (CurrentCommands, $2);
			if (!cmd)
			    ParseError ("Undefined command \"%s\"", AtomName ($2));
			else
			{
			    e = NewExprTree (OP, 
					     NewExprConst (POLY_CONST, cmd->func),
					     $3);
			    GetNamespace (&s, &f, &c);
			    t = NewThread (f, CompileExpr (e, c));
			    ThreadsRun (t, 0);
			}
			EXIT ();
		    }
		| NL
		| ENDFILE
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
rawnamespace	: rawnamespace rawatom COLONCOLON
		    { $$ = NewExprTree (COLONCOLON, $1, NewExprAtom ($2, 0, False)); }
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
fullnames	: fullname
		    {
			$$ = $1;
		    }
		| fullname COMMA fullnames
		    {
			$$ = NewExprTree (COMMA, $1, $3);
		    }
		;
namespace	: namespace NAMESPACENAME COLONCOLON
		    { 
			ExprPtr	    e;
			SymbolPtr   symbol;

			e = BuildFullname ($1, $2);
			if (e->base.tag == COLONCOLON)
			    symbol = e->tree.right->atom.symbol;
			else
			    symbol = e->atom.symbol;
			if (!symbol)
			{
			    ParseError ("non-existant namespace \"%A\"", $2);
			    YYERROR;
			}
			else if (symbol->symbol.class != class_namespace)
			{
			    ParseError ("%A is not a namespace", $2);
			    YYERROR;
			}
			LexNamespace = symbol->namespace.namespace;
			$$ = e;
		    }
		| NAMESPACENAME COLONCOLON
		    { 
			ExprPtr	    e;
			SymbolPtr   symbol;

			e = BuildFullname (0, $1);
			if (e->base.tag == COLONCOLON)
			    symbol = e->tree.right->atom.symbol;
			else
			    symbol = e->atom.symbol;
			if (!symbol)
			{
			    ParseError ("non-existant namespace \"%A\"", $1);
			    YYERROR;
			}
			else if (symbol->symbol.class != class_namespace)
			{
			    ParseError ("%A is not a namespace", $1);
			    YYERROR;
			}
			LexNamespace = symbol->namespace.namespace;
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
block		: block_start statements block_end
		    { $$ = $2; }
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
if_statement	: IF ignorenl namespace_start OP expr CP statement %prec IF
		    { $$ = NewExprTree(IF, $5, $7); }
		| IF ignorenl namespace_start OP expr CP statement ELSE statement
		    { $$ = NewExprTree(ELSE, $5, NewExprTree(ELSE, $7, $9)); }

statement:	simple_statement
		| block
		;

simple_statement: if_statement namespace_end attendnl
                    { $$ = $1; }
		| WHILE ignorenl namespace_start OP expr CP statement namespace_end attendnl
		    { $$ = NewExprTree(WHILE, $5, $7); }
		| DO ignorenl namespace_start statement WHILE OP expr CP namespace_end attendnl
		    { $$ = NewExprTree(DO, $4, $7); }
		| FOR ignorenl namespace_start OP for_exprs CP statement namespace_end attendnl
		    { Expr *expr = ParseCanonFor($5);
		      if (!expr)
                          YYERROR;
		      $$ = NewExprTree(FOR, expr, $7); }
		| SWITCH ignorenl namespace_start OP expr CP case_block namespace_end attendnl
		    { $$ = NewExprTree (SWITCH, $5, $7); }
		| union_or_enum SWITCH ignorenl namespace_start OP expr CP union_case_block namespace_end attendnl
		    { $$ = NewExprTree (UNION, $6, $8); }
		| BREAK SEMI
		    { $$ = NewExprTree(BREAK, (Expr *) 0, (Expr *) 0); }
		| CONTINUE SEMI
		    { $$ = NewExprTree(CONTINUE, (Expr *) 0, (Expr *) 0); }
		| RETURNTOK opt_expr SEMI
		    { $$ = NewExprTree (RETURNTOK, (Expr *) 0, $2); }
		| expr SEMI
		    { $$ = NewExprTree(EXPR, $1, (Expr *) 0); }
		| SEMI
		    { $$ = NewExprTree(SEMI, (Expr *) 0, (Expr *) 0); }
		| func_decl doc_string opt_func_body namespace_end
		    { 
			DeclList    *decl = $1.decl;
			SymbolPtr   symbol = decl->symbol;
			Class	    class = $1.type.class;
			Publish	    publish = $1.type.publish;
			TypePtr	    type = $1.type.type;
			TypePtr	    ret = type->func.ret;
			ArgType	    *argType = type->func.args;

			if (symbol)
			{
			    if ($3)
			    {
				symbol->symbol.forward = False;
				ParseCanonType (ret, False);
				decl->init = NewExprCode (NewFuncCode (ret,
								       argType,
								       $3,
								       $2),
							  NewExprAtom (symbol->symbol.name, symbol, False));
			    }
			    else
				symbol->symbol.forward = True;
			}
			$$ = NewExprDecl (FUNC, decl, class, type, publish);
		    }
		| opt_publish EXCEPTION ignorenl NAME namespace_start opt_argdecls namespace_end doc_string SEMI attendnl
		    { 
			DeclListPtr decl;

			decl = NewDeclList ($4, 0, 0);
			decl->symbol = ParseNewSymbol ($1, 
						       class_exception, 
						       typePoly, $4);
			decl->symbol->exception.doc = $8;
			$$ = NewExprDecl (EXCEPTION,
					  decl,
					  class_exception,
					  NewTypeFunc (typePoly, $6),
					  $1);
		    }
		| RAISE fullname OP opt_exprs CP SEMI
		    { $$ = NewExprTree (RAISE, $2, $4); }
		| opt_publish TYPEDEF ignorenl typenames SEMI attendnl
		    { 
			DeclListPtr decl;

			for (decl = $4; decl; decl = decl->next)
			    decl->symbol = ParseNewSymbol ($1, class_typedef,
							   0, decl->name);
			$$ = NewExprTree (TYPEDEF, NewExprDecl (TYPEDEF, $4, class_typedef, 0, $1), 0);
		    }
		| opt_publish TYPEDEF ignorenl type typenames SEMI attendnl
		    { 
			DeclListPtr decl;

			for (decl = $5; decl; decl = decl->next)
			    decl->symbol = ParseNewSymbol ($1, class_typedef,
							   $4, decl->name);
			$$ = NewExprTree (TYPEDEF, NewExprDecl (TYPEDEF, $5, class_typedef, $4, $1), 0);
		    }
		| publish_extend NAMESPACE ignorenl namespacename
		    {
			SymbolPtr   symbol;
			Publish	    publish = $1;
			
			/*
			 * this is a hack - save the current namespace
			 * on the parser stack to be restored after
			 * the block is compiled.
			 */
			$2 = CurrentNamespace;
			if (publish == publish_extend)
			{
			    symbol = NamespaceFindName (CurrentNamespace, $4, True);
			    if (!symbol)
			    {
				ParseError ("non-existant namespace %A", $4);
				YYERROR;
			    }
			    else if (symbol->symbol.class != class_namespace)
			    {
				ParseError ("%A is not a namespace", $4);
				YYERROR;
			    }
			    else
				CurrentNamespace = symbol->namespace.namespace;
			}
			else
			{
			    symbol = ParseNewSymbol ($1, class_namespace,
						     0, $4);
			    CurrentNamespace = NewNamespace (CurrentNamespace);
			    symbol->namespace.namespace = CurrentNamespace;
			}
			/*
			 * Make all of the symbols visible while compiling within
			 * the namespace
			 */
			if (CurrentNamespace != $2)
			    CurrentNamespace->publish = publish_public;
		    }
			OC statements CC attendnl
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
			$$ = NewExprTree (NAMESPACE, NewExprAtom ($4, 0, False), $7);
		    }
		| opt_publish IMPORT ignorenl fullnames SEMI attendnl
		    {
			SymbolPtr	symbol;
			ExprPtr		p, e, n;

			p = $4;
			for (p = $4; p; p = n)
			{
			    if (p->base.tag == COMMA)
			    {
				e = p->tree.left;
				n = p->tree.right;
			    }
			    else
			    {
				e = p;
				n = 0;
			    }
			    if (e->base.tag == COLONCOLON)
				e = e->tree.right;
			    symbol = e->atom.symbol;
			    if (!symbol)
			    {
				ParseError ("non-existant namespace %A", e->atom.atom);
				YYERROR;
			    }
			    else if (symbol->symbol.class != class_namespace)
			    {
				ParseError ("%A is not a namespace", e->atom.atom);
				YYERROR;
			    }
			    NamespaceImport (CurrentNamespace, 
					     symbol->namespace.namespace, $1);
			}
			$$ = NewExprTree (IMPORT, $4, NewExprDecl (IMPORT,
								   0, 
								   class_undef,
								   0,
								   $1));
		    }
		| try_statement attendnl
		    { $$ = $1; }
		| TWIXT ignorenl namespace_start OP opt_expr SEMI opt_expr CP statement namespace_end attendnl
		    { $$ = NewExprTree (TWIXT, 
					NewExprTree (TWIXT, $5, $7),
					NewExprTree (TWIXT, $9, 0));
		    }
		;
for_exprs       : opt_expr SEMI for_exprs
                    { $$ = NewExprTree(SEMI, $1, $3); }
		| opt_expr
		    { $$ = NewExprTree(SEMI, $1, 0); }
		;

try_body_statement:  simple_statement
		| OC statements CC
		    { $$ = $2; }

try_statement:  TRY ignorenl try_body_statement catches
		    { $$ = NewExprTree (CATCH, $4, $3); }
                ;

catches		:   catches catch %prec TRY
		    { $$ = NewExprTree (CATCH, $1, $2); }
		|   
		    { $$ = 0; }
		;
catch		: CATCH fullname namespace_start args doc_string block namespace_end
		    { $$ = NewExprCode (NewFuncCode (typePrim[rep_void],
						     $4, $6, $5),
					$2); 
		    }
		;
opt_func_body	: func_body
		| SEMI
		    { $$ = 0; }
		;
func_body    	: { ++funcDepth; } block_or_expr { --funcDepth; $$ = $2; }
		;
block_or_expr	: block
		    { $$ = $1; }
		| attendnl ASSIGN simpleexpr SEMI
		    { 
			$$ = NewExprTree (OC,
					  NewExprTree (RETURNTOK, 0, $3),
					  NewExprTree (OC, 0, 0));
		    }
		;
union_or_enum	: UNION
		| ENUM
		;
see_comment	:
		    { seeComment = 1; }
		;
doc_string	: see_comment opt_comment 
		    { seeComment = 0; $$ = $2; }
		;
opt_comment	: COMMENT_CONST
		|
		    { $$ = Void; }
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
union_case	: CASE namespace_start NAME opt_name COLON statements namespace_end
		    { 
			SymbolPtr   sym = $4;
			Atom	    sym_atom = sym ? sym->symbol.name : 0;
			ExprPtr	    name = 0;
			
			if (sym)
			    name = NewExprAtom (sym_atom, sym, False);
			    
			$$ = NewExprTree (CASE, 
					  NewExprTree (CASE,
						       NewExprAtom ($3, 0, False),
						       name),
					  $6);
		    }
		| DEFAULT COLON statements
		    { $$ = NewExprTree (CASE, 0, $3); }
		;
opt_name	: NAME
		    {
			$$ = ParseNewSymbol (publish_private,
					     class_undef,
					     typePoly,
					     $1);
		    }
		|
		    { $$ = 0; }
		;
/*
* Identifiers
*/
atoms		: NAME COMMA atoms
		    { $$ = NewAtomList ($3, $1); }
		| NAME
		    { $$ = NewAtomList (0, $1); }
		;
typenames	: typename COMMA typenames
		    { $$ = NewDeclList ($1, 0, $3); }
		| typename
		    { $$ = NewDeclList ($1, 0, 0); }
		;
typename	: TYPENAME
		| NAME
		;

/*
 * Ok, a few cute hacks to fetch the fulltype from the
 * value stack -- initnames always immediately follows a decl,
 * so $0 will be a fulltype.  Note the cute trick to store the
 * type across the comma operator -- this means that name
 * will always find the fulltype at $0 as well
 */
initnames	: name opt_init next_decl initnames
		    { 
			if ($1)
			{
			    $$ = NewDeclList ($1->symbol.name, $2, $4); 
			    $$->symbol = $1;
			}
			else
			    $$ = $4;
		    }
		| name opt_init
		    { 
			if ($1)
			{
			    $$ = NewDeclList ($1->symbol.name, $2, 0);
			    $$->symbol = $1;
			}
			else
			    $1 = 0;
		    }
		;
name		: NAME
		    {	
			$$ = ParseNewSymbol ($<fulltype>0.publish,
					     $<fulltype>0.class,
					     $<fulltype>0.type,
					     $1);
		    }
		;
/* 
 * next_decl -- look backwards three entries on the stack to
 * find the previous type
 */
next_decl	: COMMA
		    { $$ = $<fulltype>-2; }
		;
/*
 * Declaration of a function
 */
func_decl	: func_name namespace_start opt_argdefines CP
		    {
			NamespacePtr	save = CurrentNamespace;
			SymbolPtr	symbol;
			Type		*type = NewTypeFunc ($1.type.type, $3);
			/*
			 * namespace_start pushed a new namespace, make sure
			 * this symbol is placed in the original namespace
			 */
			CurrentNamespace = save->previous;
			symbol = NamespaceFindName (CurrentNamespace, $1.decl->name, True);
			if (symbol && symbol->symbol.forward)
			{
			    if (!TypeIsSupertype (symbol->symbol.type, type))
			    {
				ParseError ("%A redefinition with different type",
					    $1.decl->name);
				symbol = 0;
			    }
			}
			else
			{
			    symbol = ParseNewSymbol ($1.type.publish,
						     $1.type.class, 
						     type,
						     $1.decl->name);
			}
			CurrentNamespace = save;
			$$ = $1;
			$$.decl->symbol = symbol;
			$$.type.type = type;
		    }
		;
func_name	: decl NAME OP
		    { $$.type = $1; $$.decl = NewDeclList ($2, 0, 0); }
		| decl FUNCTION NAME OP
		    { $$.type = $1; $$.decl = NewDeclList ($3, 0, 0); }
		| FUNCTION ignorenl NAME OP
		    { 
			$$.type.publish = publish_private;
			$$.type.class = class_undef;
			$$.type.type = typePoly;
			$$.decl = NewDeclList ($3, 0, 0); 
		    }
		;
opt_init	: ASSIGN simpleexpr
		    { $$ = $2; }
		| ASSIGN init
		    { $$ = $2; }
		|
		    { $$ = 0; }
		;
/*
 * Full declaration including storage, type and publication
 */
decl		: publish opt_class opt_type opt_nl
		    { $$.publish = $1; $$.class = $2; $$.type = $3; }
		| class opt_type opt_nl
		    { $$.publish = publish_private; $$.class = $1; $$.type = $2; }
		| type opt_nl
		    { $$.publish = publish_private; $$.class = class_undef; $$.type = $1; }
		;
/*
 * Type declarations
 */
subscripts	: opt_argdecls subscripts	    %prec CALL
		    { $$ = NewTypeFunc ($2, $1); }
		| OS opt_stars CS subscripts    
		    { $$ = NewTypeArray ($4, $2, False); }
		| OS dotdotdots CS subscripts    
		    { $$ = NewTypeArray ($4, $2, True); }
		| OS dims CS subscripts
		    { $$ = NewTypeArray ($4, $2, False); }
		| OS type CS subscripts
		    { $$ = NewTypeHash ($4, $2); }
		|
		    { $$ = 0; }
		;
type		: subtype subscripts	    %prec CALL
		    {
			Type	*top = $2;
			Type	*t, **bot = &top;

			/*
			 * Walk down the type chain to hang the
			 * base type off of the end.  This
			 * makes int[]() be an array of functions
			 * rather than a function returning an array
			 */
			while ((t = *bot))
			    switch (t->base.tag) {
			    case type_array:
				bot = &t->array.type;
				break;
			    case type_hash:
				bot = &t->hash.type;
				break;
			    case type_func:
				bot = &t->func.ret;
				break;
			    default:
				assert(0);
			    }
			*bot = $1;
			$$ = top;
		    }
		| TIMES type			%prec POINTER
		    { $$ = NewTypeRef ($2, True); }
		| STARSTAR type			%prec POINTER
		    { $$ = NewTypeRef (NewTypeRef ($2, True), True); }
		| AND type			%prec POINTER
		    { $$ = NewTypeRef (NewTypeRef ($2, False), False); }
		| LAND type			%prec POINTER
		    { $$ = NewTypeRef ($2, False); }
		| type PLUS type
		    { 
			if (ParseCanonType ($1, False) != CanonTypeDefined)
			    YYERROR;
			if (ParseCanonType ($3, False) != CanonTypeDefined)
			    YYERROR;
			$$ = NewTypePlus ($1, $3); 
			if (!$$)
			    YYERROR;
		    }
		;
subtype		: basetype
		| STRUCT OC struct_members CC
		    {
			AtomListPtr	al;
			StructType	*st;
			MemListPtr	ml;
			int		nelements;

			nelements = 0;
			for (ml = $3; ml; ml = ml->next)
			{
			    for (al = ml->atoms; al; al = al->next)
				nelements++;
			}
			st = NewStructType (nelements);
			nelements = 0;
			for (ml = $3; ml; ml = ml->next)
			{
			    for (al = ml->atoms; al; al = al->next)
			    {
				AddBoxType (&st->types, ml->type);
				StructTypeAtoms(st)[nelements] = al->atom;
				nelements++;
			    }
			}
			$$ = NewTypeStruct (st);
		    }
		| UNION OC union_members CC
		    {
			AtomListPtr	al;
			StructType	*st;
			MemListPtr	ml;
			int		nelements;

			nelements = 0;
			for (ml = $3; ml; ml = ml->next)
			{
			    for (al = ml->atoms; al; al = al->next)
				nelements++;
			}
			st = NewStructType (nelements);
			nelements = 0;
			for (ml = $3; ml; ml = ml->next)
			{
			    for (al = ml->atoms; al; al = al->next)
			    {
				AddBoxType (&st->types, ml->type);
				StructTypeAtoms(st)[nelements] = al->atom;
				nelements++;
			    }
			}
			$$ = NewTypeUnion (st, False);
		    }
		| ENUM OC atoms CC
		    {
			AtomListPtr	al;
			StructType	*st;
			int		nelements;

			nelements = 0;
			for (al = $3; al; al = al->next)
			    nelements++;
			
			st = NewStructType (nelements);
			nelements = 0;
			for (al = $3; al; al = al->next)
			{
			    AddBoxType (&st->types, typePrim[rep_void]);
			    StructTypeAtoms(st)[nelements] = al->atom;
			    nelements++;
			}
			$$ = NewTypeUnion (st, True);
		    }
			    
		| OP type CP
		    { $$ = $2; }
		| fulltype
		    { $$ = NewTypeName ($1, 0); }
		;
opt_type	: type
		|
		    { $$ = typePoly; }
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
		| BOOL
		| FOREIGN
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
dotdotdots	: dotdotdots COMMA DOTDOTDOT
		    { $$ = NewExprTree (COMMA, 0, $1); }
		| DOTDOTDOT
		    { $$ = NewExprTree (COMMA, 0, 0); }
		;
dims		: simpleexpr COMMA dims
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| simpleexpr
		    { $$ = NewExprTree (COMMA, $1, 0); }
		;
/*
* Structure member declarations
*/
struct_members	: opt_type atoms SEMI struct_members
		    { $$ = NewMemList ($2, $1, $4); }
		|
		    { $$ = 0; }
		;
union_members	: opt_type atoms SEMI union_members
		    { $$ = NewMemList ($2, $1, $4); }
		|
		    { $$ = 0; }
		;
/*
* Declaration modifiers
*/
opt_class	: class
		|
		    { $$ = class_undef; }
		;
class		: GLOBAL
		| AUTO
		| STATIC
		| CONST
		;
opt_publish	: publish
		|
		    { $$ = publish_private; }
		;

publish		: PUBLIC
		| PROTECTED
		;
publish_extend	: opt_publish
		| EXTEND
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
		    { $$ = NewArgType ($1.type, False, $1.name, 0, $3); }
		| argdecl opt_dotdotdot
		    { $$ = NewArgType ($1.type, $2, $1.name, 0, 0); }
		;
argdecl		: type NAME
		    { 
			ParseCanonType ($1, False);
			$$.type = $1; 
			$$.name = $2; 
		    }
		| type
		    { 
			ParseCanonType ($1, False);
			$$.type = $1;
			$$.name = 0; 
		    }
		;

/*
* Arguments in function definitions
*/
args		: OP opt_argdefines CP
		    { $$ = $2; }
		;
opt_argdefines	: ignorenl argdefines
		    {
			ArgType	*args;
			Type	*type;

			for (args = $2; args; args = args->next)
			{
			    type = args->type;
			    if (ParseCanonType (type, False) != CanonTypeDefined)
				break;
			    if (args->varargs)
			    {
				type = NewTypeArray (type,
						     NewExprTree (COMMA,
								  NewExprConst (TEN_NUM, 
										NewInt (0)),
								  0),
						     False);
			    }
			    args->symbol = ParseNewSymbol (publish_private,
							   class_arg, 
							   type, args->name);
			}
			$$ = $2;
		    }
		| ignorenl
		    { $$ = 0; }
		;
argdefines	: argdefine COMMA argdefines
		    { $$ = NewArgType ($1.type, False, $1.name, 0, $3); }
		| argdefine opt_dotdotdot
		    { $$ = NewArgType ($1.type, $2, $1.name, 0, 0); }
		;
argdefine	: opt_type NAME
		    { $$.type = $1; $$.name = $2; }
		| type
		    { $$.type = $1; $$.name = 0; }
		;
opt_dotdotdot	: DOTDOTDOT
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
			    if (decl->init)
			    {
				if (!decl->init->base.type)
				    decl->init->base.type = $1.type;
			    }
			}

			$$ = NewExprDecl (VAR, $2, $1.class, $1.type, $1.publish); 
		    }
		;
comma_expr	: simpleexpr
		| comma_expr COMMA simpleexpr
		    { $$ = NewExprTree(COMMA, $1, $3); }
		;
/*
* Expression list, different use of commas for function arguments et al.
*/
opt_exprs	: exprs
		|
		    { $$ = 0; }
		;
exprs		: simpleexpr COMMA exprs
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| simpleexpr
		    { $$ = NewExprTree (COMMA, $1, 0); }
		;
opt_actuals	: actuals
		|
		    { $$ = 0; }
		;
actuals		: simpleexpr COMMA actuals
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| simpleexpr opt_dotdotdot
		    {	
			ExprPtr	arg = $2 ? NewExprTree (DOTDOTDOT, $1, 0) : $1;
			$$ = NewExprTree (COMMA, arg, 0); 
		    }
		;
func_right	: attendnl ASSIGN simpleexpr
		    { 
			$$ = NewExprTree (OC,
					  NewExprTree (RETURNTOK, 0, $3),
					  NewExprTree (OC, 0, 0));
		    }
		| { ++funcDepth; } block { --funcDepth; $$ = $2; }
		;
/*
* Fundemental expression production
*/
simpleexpr	: simpleexpr assignop simpleexpr    		%prec ASSIGN
		    { 
			if ($2 == ASSIGNPOW)
			    $$ = NewExprTree (ASSIGNPOW, 
					      BuildName ("Math", "assign_pow"),
					      NewExprTree (ASSIGNPOW, $1, $3));
			else
			{
			    ExprPtr left = $1;
			    /*
			     * Automatically declare names used in
			     * simple assignements at the top level
			     */
			    if ($2 == ASSIGN && 
				funcDepth == 0 &&
				left->base.tag == NAME && 
				!(left->atom.symbol))
			    {
				$1->atom.symbol = ParseNewSymbol (publish_private,
								  class_undef, 
								  typePoly, 
								  $1->atom.atom);
			    }
			    $$ = NewExprTree($2, $1, $3); 
			}
		    }
		| opt_type FUNC namespace_start args doc_string func_right namespace_end	    	%prec ASSIGN
		    {
			ParseCanonType ($1, False);
			$$ = NewExprCode (NewFuncCode ($1, $4, $6, $5), 0); 
		    }
		| MOD integer						%prec THREADID
		    {   Value	t;
			t = do_Thread_id_to_thread ($2);
			if (t == Void)
			{
			    ParseError ("No thread %v", $2);
			    YYERROR;
			}
			else
			    $$ = NewExprConst(THREAD_CONST, t); 
		    }
		| TIMES simpleexpr					%prec STAR
		    { $$ = NewExprTree(STAR, $2, (Expr *) 0); }
		| STARSTAR simpleexpr					%prec STAR
		    { $$ = NewExprTree(STAR, NewExprTree (STAR, $2, 0), 0); }
		| LAND simpleexpr					%prec AMPER
		    { $$ = NewExprTree(AMPER, $2, (Expr *) 0); }
		| AND simpleexpr					%prec AMPER
		    { $$ = NewExprTree(AMPER, 
				       NewExprTree (AMPER, $2, (Expr *) 0), 
				       (Expr *) 0); }
		| MINUS simpleexpr					%prec UMINUS
		    { $$ = NewExprTree(UMINUS, $2, (Expr *) 0); }
		| LNOT simpleexpr
		    { $$ = NewExprTree(LNOT, $2, (Expr *) 0); }
		| BANG simpleexpr
		    { $$ = NewExprTree(BANG, $2, (Expr *) 0); }
		| simpleexpr BANG					%prec FACT
		    { 
			$$ = NewExprTree(FACT, 
					 BuildName ("Math", "factorial"),
					 $1);
		    }
		| INC simpleexpr
		    { $$ = NewExprTree(INC, $2, (Expr *) 0); }
		| simpleexpr INC
		    { $$ = NewExprTree(INC, (Expr *) 0, $1); }
		| DEC simpleexpr
		    { $$ = NewExprTree(DEC, $2, (Expr *) 0); }
		| simpleexpr DEC
		    { $$ = NewExprTree(DEC, (Expr *) 0, $1); }
		| FORK simpleexpr
		    { $$ = NewExprTree (FORK, (Expr *) 0, $2); }
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
		| simpleexpr STARSTAR simpleexpr			%prec POW
		    { 
			$$ = NewExprTree(POW, 
					 BuildName ("Math", "pow"), 
					 NewExprTree (POW, $1, $3)); 
		    }
		| simpleexpr POW2
		    {
			$$ = NewExprTree (POW,
					  BuildName("Math", "pow"),
					  NewExprTree (POW, $1,
						       NewExprConst (TEN_NUM,
								     NewInt(2))));
		    }
		| simpleexpr POW3
		    {
			$$ = NewExprTree (POW,
					  BuildName("Math", "pow"),
					  NewExprTree (POW, $1,
						       NewExprConst (TEN_NUM,
								     NewInt(3))));
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
		| fullname
		| TEN_NUM
		    { $$ = NewExprConst(TEN_NUM, $1); }
		| OCTAL_NUM
		    { $$ = NewExprConst(OCTAL_NUM, $1); }
		| OCTAL0_NUM
		    { $$ = NewExprConst(OCTAL0_NUM, $1); }
		| BINARY_NUM
		    { $$ = NewExprConst(BINARY_NUM, $1); }
		| HEX_NUM
		    { $$ = NewExprConst(HEX_NUM, $1); }
		| TEN_FLOAT
		    { $$ = NewExprConst(TEN_FLOAT, $1); }
		| OCTAL_FLOAT
		    { $$ = NewExprConst(OCTAL_FLOAT, $1); }
		| OCTAL0_FLOAT
		    { $$ = NewExprConst(OCTAL0_FLOAT, $1); }
		| BINARY_FLOAT
		    { $$ = NewExprConst(BINARY_FLOAT, $1); }
		| HEX_FLOAT
		    { $$ = NewExprConst(HEX_FLOAT, $1); }
		| CHAR_CONST
		    { $$ = NewExprConst(CHAR_CONST, $1); }
		| STRING_CONST
		    { $$ = NewExprConst(STRING_CONST, $1); }
		| VOIDVAL
		    { $$ = NewExprConst(VOIDVAL, $1); }
		| BOOLVAL
		    { $$ = NewExprConst(BOOLVAL, $1); }
		| OS CS
		    { 
			$$ = BuildFullname (0, AtomId ("[]"));
		    }
		| OP type CP namespace_start init namespace_end
		    { 
			ParseCanonType ($2, False);
			if ($5)
			    $5->base.type = $2;
			$$ = NewExprTree (NEW, $5, 0);
			$$->base.type = $2;
		    }
		| OP OS stars CS CP namespace_start arrayinit namespace_end
		    { 
			$7->base.type = NewTypeArray (typePoly, $3, True); 
			ParseCanonType ($7->base.type, False);
			$$ = NewExprTree (NEW, $7, 0); 
			$$->base.type = $7->base.type;
		    }
		| OP OS dims CS CP namespace_start opt_arrayinit namespace_end
		    { 
			TypePtr	t = NewTypeArray (typePoly, $3, False);
			ParseCanonType (t, False);
			$$ = NewExprTree (NEW, $7, 0); 
			if ($7)
			    $7->base.type = t;
			$$->base.type = t;
		    }
		| OP OS type CS CP namespace_start opt_hashinit namespace_end
		    {
			TypePtr t = NewTypeHash (typePoly, $3);
			ParseCanonType (t, False);
			$$ = NewExprTree (NEW, $7, 0);
			if ($7)
			    $7->base.type = t;
			$$->base.type = t;
		    }
		| type DOT NAME						%prec UNIONCAST
		    {
			ParseCanonType ($1, False);
			$$ = NewExprTree (UNION, NewExprAtom ($3, 0, False), 0); 
			$$->base.type = $1;
		    }
		| OP type DOT NAME CP simpleexpr 			%prec UNIONCAST
		    { 
			ParseCanonType ($2, False);
			$$ = NewExprTree (UNION, NewExprAtom ($4, 0, False), $6); 
			$$->base.type = $2;
		    }
		| DOLLAR opt_integer
		    { $$ = BuildCall ("History", "fetch", 1, NewExprConst (TEN_NUM, $2)); }
		| DOT
		    { $$ = NewExprTree (DOLLAR, 0, 0); }
		| OP expr CP
		    { $$ = $2; }
		| OP block CP
		    { $$ = $2; }
		| simpleexpr STAROS dims CS
		    { $$ = NewExprTree (OS, NewExprTree (STAR, $1, (Expr *) 0), $3); }
		| simpleexpr OS dims CS
		    { $$ = NewExprTree(OS, $1, $3); }
		| simpleexpr OP opt_actuals CP				%prec CALL
		    { $$ = NewExprTree (OP, $1, $3); }
		| ISTYPE OP simpleexpr COMMA type CP			%prec CALL
		    {
			TypePtr	type = $5;
			ParseCanonType (type, False);
			$$ = NewExprType (ISTYPE, $3, type);
		    }
		| HASMEMBER OP simpleexpr COMMA NAME CP			%prec CALL
		    { $$ = NewExprTree (HASMEMBER, $3, NewExprAtom($5, 0, False)); }
		| simpleexpr DOT NAME
		    { $$ = NewExprTree(DOT, $1, NewExprAtom ($3, 0, False)); }
		| simpleexpr ARROW NAME
		    { $$ = NewExprTree(ARROW, $1, NewExprAtom ($3, 0, False)); }
		;
opt_integer	: integer
		|
		    { $$ = Zero; }
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
		| ASSIGNOR
		| ASSIGNAND
		| ASSIGN
		;
integer		: TEN_NUM
		| OCTAL_NUM
		| OCTAL0_NUM
		| BINARY_NUM
		| HEX_NUM
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
arrayinit    	: OC arrayelts opt_comma opt_dotdotdot CC
		    { 
			ExprPtr	elts = $2 ? ExprRehang ($2, 0) : 0;
			if ($4)
			{
			    ExprPtr i = elts;
			    while (i->tree.right)
				i = i->tree.right;
			    i->tree.right = NewExprTree (COMMA, 
							 NewExprTree (DOTDOTDOT, 0, 0),
							 0);
			}
			$$ = NewExprTree (ARRAY, elts, 0); 
		    }
		| OC OS namespace_start compnames comparray CS comprehension namespace_end CC
		    {
			$$ = NewExprTree (COMP, NewExprTree (COMP, $4, $5), $7);
		    }
		;
comprehension	:   ASSIGN arrayelt
		    { $$ = $2; }
		|   block
		;
compnames	: compname COMMA compnames
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| compname
		;
compname	: NAME
		    { 
			SymbolPtr   s;
			s = ParseNewSymbol (publish_private, class_arg, 
					    typePrim[rep_integer], $1);
			$$ = NewExprAtom ($1, s, False); 
		    }
		;
comparray	:
		    {
			SymbolPtr   s;
			Atom	    a = AtomId("[]");
			s = ParseNewSymbol (publish_private, class_undef,
					    typePoly, a);
			$$ = NewExprAtom (a, s, False);
		    }
		;
arrayelts	: arrayelts COMMA arrayelt
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| arrayelt
		    { $$ = NewExprTree (COMMA, 0, $1); }
		;
arrayelt	: simpleexpr
		| init
		;

/*
 * Hash initializers
 */
opt_hashinit	: hashinit
		| OC CC
		    { $$ = 0; }
		|
		    { $$ = 0; }
		;
hashinit	: OC hashelts opt_comma CC
		    {
			ExprPtr elts = $2 ? ExprRehang ($2, 0) : 0;
			$$ = NewExprTree (HASH, elts, 0);
		    }
		;
hashelts	: hashelts COMMA hashelt
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| hashelt
		    { $$ = NewExprTree (COMMA, 0, $1); }
		;
hashelt		: simpleexpr DARROW hashvalue
		    { $$ = NewExprTree (DARROW, $1, $3); }
		| DARROW hashvalue
		    { $$ = NewExprTree (DARROW, 0, $2); }
		;
hashvalue	: simpleexpr
		| init
		;
/* 
* Structure initializers
*/
structinit    	: OC structelts opt_comma CC
		    { $$ = NewExprTree (STRUCT, ExprRehang ($2, 0), 0); }
		;
structelts	: structelts COMMA structelt
		    { $$ = NewExprTree (COMMA, $1, $3); }
		| structelt
		    { $$ = NewExprTree (COMMA, 0, $1); }
		;
structelt	: NAME ASSIGN simpleexpr
		    { $$ = NewExprTree (ASSIGN, NewExprAtom ($1, 0, False), $3); }
		| NAME ASSIGN init
		    { $$ = NewExprTree (ASSIGN, NewExprAtom ($1, 0, False), $3); }
		| DOT NAME ASSIGN simpleexpr
		    { $$ = NewExprTree (ASSIGN, NewExprAtom ($2, 0, False), $4); }
		| DOT NAME ASSIGN init
		    { $$ = NewExprTree (ASSIGN, NewExprAtom ($2, 0, False), $4); }
		;
init		: arrayinit
		| structinit
		| hashinit
		| OC CC
		    { $$ = NewExprTree (ANONINIT, 0, 0); }
		;
%%

static void
DeclListMark (void *object)
{
    DeclListPtr	dl = object;

    MemReference (dl->next);
    MemReference (dl->init);
}

DataType DeclListType = { DeclListMark, 0, "DeclListType" };

DeclListPtr
NewDeclList (Atom name, ExprPtr init, DeclListPtr next)
{
    ENTER ();
    DeclListPtr	dl;

    dl = ALLOCATE (&DeclListType, sizeof (DeclList));
    dl->next = next;
    dl->name = name;
    dl->symbol = 0;
    dl->init = init;
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

DataType MemListType = { MemListMark, 0, "MemListType" };

MemListPtr
NewMemList (AtomListPtr atoms, Type *type, MemListPtr next)
{
    ENTER ();
    MemListPtr	ml;

    ml = ALLOCATE (&MemListType, sizeof (MemList));
    ml->next = next;
    ml->type = type;
    ml->atoms = atoms;
    RETURN (ml);
}


extern NamespacePtr	CurrentNamespace;
FramePtr	CurrentFrame;

Value
lookupVar (char *ns, char *n)
{
    ENTER ();
    Value	    v;
    SymbolPtr	    symbol;
    NamespacePtr    namespace;
    Bool	    search;

    search = True;
    namespace = CurrentNamespace;
    if (ns)
    {
	symbol = NamespaceFindName (CurrentNamespace, AtomId (ns), True);
	if (symbol && symbol->symbol.class == class_namespace)
	    namespace = symbol->namespace.namespace;
	else
	    namespace = 0;
    }
    if (namespace)
	symbol = NamespaceFindName (namespace, AtomId (n), search);
    else
	symbol = 0;
    if (symbol && symbol->symbol.class == class_global)
	v = BoxValue (symbol->global.value, 0);
    else
	v = Void;
    RETURN (v);
}

void
setVar (NamespacePtr namespace, char *n, Value v, Type *type)
{
    ENTER ();
    Atom	atom;
    SymbolPtr	symbol;

    atom = AtomId (n);
    symbol = NamespaceFindName (namespace, atom, True);
    if (!symbol)
	symbol = NamespaceAddName (namespace,
				   NewSymbolGlobal (atom, type),
				   publish_private);
    if (symbol->symbol.class == class_global)
	BoxValueSet (symbol->global.value, 0, v);
    EXIT ();
}

void
GetNamespace (NamespacePtr *scope, FramePtr *fp, CodePtr *cp)
{
    *scope = CurrentNamespace;
    *fp = CurrentFrame;
    if (CurrentFrame)
	*cp = CurrentFrame->function->func.code;
    else
	*cp = 0;
}

ExprPtr
BuildName (char *ns, char *n)
{
    ENTER ();
    SymbolPtr	    symbol, ns_symbol = 0;
    Atom	    atom, ns_atom = 0;
    Bool	    search = True;
    NamespacePtr    namespace = CurrentNamespace;
    ExprPtr	    e;
    Bool	    ns_privateFound = False;
    Bool	    privateFound = False;

    if (ns)
    {
	ns_atom = AtomId (ns);
	ns_symbol = NamespaceFindName (namespace, ns_atom, search);
	if (ns_symbol && ns_symbol->symbol.class == class_namespace)
	    namespace = ns_symbol->namespace.namespace;
	else
	{
	    if (!ns_symbol)
		ns_privateFound = NamespaceIsNamePrivate (namespace, ns_atom, search);
	    namespace = 0;
	}
	search = False;
    }
    atom = AtomId (n);
    if (namespace)
    {
	symbol = NamespaceFindName (namespace, atom, search);
	if (!symbol)
	    privateFound = NamespaceIsNamePrivate (namespace, atom, search);
    }
    else
	symbol = 0;
    e = NewExprAtom (atom, symbol, privateFound);
    if (ns_atom)
	e = NewExprTree (COLONCOLON, NewExprAtom (ns_atom, ns_symbol, ns_privateFound), e);
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
    array = NewArray (False, False, typePrim[rep_string], 1, &len);
    len--;
    ArrayValueSet (&array->array, len, AtomString (name));
    e = colonnames;
    while (--len >= 0)
    {
	ArrayValueSet (&array->array, len, AtomString (e->tree.right->atom.atom));
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
    SymbolPtr	    symbol;
    Bool	    search;
    ExprPtr	    nameExpr;
    Bool	    privateFound = False;

    if (left)
    {
	if (left->base.tag == COLONCOLON)
	    symbol = left->tree.right->atom.symbol;
	else
	    symbol = left->atom.symbol;
	if (symbol && symbol->symbol.class == class_namespace)
	    namespace = symbol->namespace.namespace;
	else
	    namespace = 0;
        search = False;
    }
    else
    {
	namespace = CurrentNamespace;
	search = True;
    }
    if (namespace)
    {
	symbol = NamespaceFindName (namespace, atom, search);
	if (!symbol)
	    privateFound = NamespaceIsNamePrivate (namespace, atom, search);
    }
    else
	symbol = 0;
    nameExpr = NewExprAtom (atom, symbol, privateFound);
    if (left)
	nameExpr = NewExprTree (COLONCOLON, left, nameExpr);
    RETURN (nameExpr);
}

ExprPtr
BuildCall (char *scope, char *name, int nargs, ...)
{
    ENTER ();
    va_list	    alist;
    ExprPtr	    args, *prev;
    ExprPtr	    f;
    ExprPtr	    e;

    va_start (alist, nargs);
    prev = &args;
    args = 0;
    while (nargs--)
    {
	*prev = NewExprTree (COMMA, va_arg (alist, ExprPtr), 0);
	prev = &(*prev)->tree.right;
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
 * Walk for() loop arguments and normalize the list
 */
static Expr *
ParseCanonFor (Expr *expr)
{
    if (!expr || !expr->tree.right) {
        ParseError ("Too few exprs in for()\n");
        return 0;
    }
    if (!expr->tree.right->tree.right) {
        /* 2-argument for() */
        expr = NewExprTree(FOR, 0, expr);
    }
    if (expr->tree.right->tree.right->tree.right) {
        ParseError ("Too many exprs in for()\n");
        return 0;
    }
    return expr;
}

/*
 * Walk a type structure and resolve any type names
 */
static CanonTypeResult
ParseCanonType (TypePtr type, Bool forwardAllowed)
{
    ArgType	    *arg;
    int		    n;
    CanonTypeResult ret = CanonTypeDefined, t;
    Bool	    anyResolved;
    
    if (!type)
    {
	ParseError ("Type missing inside compiler");
	return False;
    }
    switch (type->base.tag) {
    case type_prim:
	break;
    case type_name:
	if (!TypeNameType(type))
	{
	    if (!type->name.name)
	    {
		ExprPtr e;
		e = type->name.expr;
		if (e->base.tag == COLONCOLON)
		    e = e->tree.right;
		type->name.name = e->atom.symbol;
		if (!type->name.name)
		{
		    ParseError ("No typedef \"%A\" in namespace",
				e->atom.atom);
		    ret = CanonTypeUndefined;
		    break;
		}
	    }
	    if (type->name.name->symbol.class != class_typedef)
	    {
		ParseError ("Symbol \"%A\" not a typedef", 
			    type->name.name->symbol.name);
		ret = CanonTypeUndefined;
	    }
	    else if (!TypeNameType(type))
	    {
		if (!forwardAllowed)
		    ParseError ("Typedef \"%A\" not defined yet", 
				type->name.name->symbol.name);
		ret = CanonTypeForward;
	    }
	    else
	    {
		ret = ParseCanonType (TypeNameType(type), forwardAllowed);
	    }
	}
	break;
    case type_ref:
	ret = ParseCanonType (type->ref.ref, True);
	if (ret == CanonTypeForward)
	    ret = CanonTypeDefined;
	break;
    case type_func:
	if (type->func.ret)
	    ret = ParseCanonType (type->func.ret, forwardAllowed);
	for (arg = type->func.args; arg; arg = arg->next)
	{
	    t = ParseCanonType (arg->type, forwardAllowed);
	    if (t < ret)
		ret = t;
	}
	break;
    case type_array:
	ret = ParseCanonType (type->array.type, forwardAllowed);
	break;
    case type_hash:
	ret = ParseCanonType (type->hash.type, forwardAllowed);
	t = ParseCanonType (type->hash.keyType, forwardAllowed);
	if (t < ret)
	    ret = t;
	break;
    case type_struct:
	for (n = 0; n < type->structs.structs->nelements; n++)
	{
	    StructType   *st = type->structs.structs;

	    t = ParseCanonType (BoxTypesElements(st->types)[n], forwardAllowed);
	    if (t < ret)
		ret = t;
	}
	break;
    case type_union:
	anyResolved = False;
	for (n = 0; n < type->structs.structs->nelements; n++)
	{
	    StructType   *st = type->structs.structs;

	    t = ParseCanonType (BoxTypesElements(st->types)[n], True);
	    if (t < ret)
		ret = t;
	    if (t == CanonTypeDefined)
		anyResolved = True;
	}
	if (ret == CanonTypeForward)
	{
	    if (anyResolved)
		ret = CanonTypeDefined;
	    else if (!forwardAllowed)
		ParseError ("No member of '%T' defined yet", type);
	}
	break;
    case type_types:
	break;
    }
    return ret;
}

static SymbolPtr
ParseNewSymbol (Publish publish, Class class, Type *type, Atom name)
{
    ENTER ();
    SymbolPtr	s = 0;
    
    if (class == class_undef)
	class = funcDepth ? class_auto : class_global;

    if (class == class_namespace || 
	(class == class_typedef && type == 0) || 	
	ParseCanonType (type, False) == CanonTypeDefined)
    {
	switch (class) {
	case class_const:
	    s = NewSymbolConst (name, type);
	    break;
	case class_global:
	    s = NewSymbolGlobal (name, type);
	    break;
	case class_static:
	    s = NewSymbolStatic (name, type);
	    break;
	case class_arg:
	    s = NewSymbolArg (name, type);
	    break;
	case class_auto:
	    s = NewSymbolAuto (name, type);
	    break;
	case class_exception:
	    s = NewSymbolException (name, type, Void);
	    break;
	case class_typedef:
	    /*
	     * Special case for typedefs --
	     * allow forward declaration of untyped
	     * typedef names, then hook the
	     * new type to the old name
	     */
	    if (type)
	    {
		s = NamespaceFindName (CurrentNamespace, name, False);
		if (s && s->symbol.class == class_typedef && !s->symbol.type)
		{
		    s->symbol.type = type;
		    RETURN (s);
		}
	    }
	    s = NewSymbolType (name, type);
	    break;
	case class_namespace:
	    s = NewSymbolNamespace (name, NewNamespace (CurrentNamespace));
	    break;
	case class_undef:
	    break;
	}
	if (s)
	    NamespaceAddName (CurrentNamespace, s, publish);
    }
    RETURN (s);
}

int
yywrap (void)
{
    if (LexInteractive())
	printf ("\n");
    if (CurrentFrame)
    {
	do_Debug_done ();
	return 0;
    }
    return 1;
}

extern	char *yytext;

void
ParseError (char *fmt, ...)
{
    va_list	args;

    if (LexFileName ())
	FilePrintf (FileStderr, "%A:%d: ",
		    LexFileName (), LexFileLine ());
    va_start (args, fmt);
    FileVPrintf (FileStderr, fmt, args);
    FilePrintf (FileStderr, "\n");
    va_end (args);
}

void
yyerror (char *msg)
{
    ignorenl = 0;
    ParseError ("%s before %S", msg, yytext);
}
