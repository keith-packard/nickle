%{

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	"gram.h"
#include	"ref.h"
#include	<strings.h>
#include	<errno.h>
#ifdef HAVE_LIBREADLINE
#include	<readline/readline.h>
#include	<readline/history.h>
#endif
    
/*
 * Silence gcc by providing prototypes for these functions
 */
int yyget_lineno (void);
FILE *yyget_in (void);
FILE *yyget_out (void);
int yyget_leng (void);
char *yyget_text (void);
void yyset_lineno (int);
void yyset_in (FILE *);
void yyset_out (FILE *);
int yyget_debug (void);
void yyset_debug (int);
int yylex_destroy (void);

typedef struct _lexInput {
    DataType		data;
    struct _lexInput	*next;
    Value		file;
    Atom		name;
    int			lineno;
    Bool		at_bol;
    Bool		at_eof;
    Bool		interactive;
} LexInput;

LexInput    *lexInput;
extern int  ignorenl;
extern int  notCommand;
extern int  seeComment;
#define	    FILE_END_CHAR	-2
    
static void lexInputMark (void *object)
{
    LexInput *lex = object;
    
    MemReference (lex->next);
    MemReference (lex->file);
}

static DataType lexInputType = {
    lexInputMark, 0, "lexInputType"
};

void
NewLexInput (Value file, Atom name, Bool after, Bool interactive)
{
    ENTER ();
    LexInput	*lex;

    lex = MemAllocate (&lexInputType, sizeof (*lex));
    lex->file = file;
    lex->name = name;
    lex->lineno = 0;
    lex->at_bol = True;
    lex->at_eof = False;
    lex->interactive = interactive;
    if (after)
    {
	LexInput **prev;
	
	for (prev = &lexInput; *prev; prev = &(*prev)->next)
	    ;
	lex->next = 0;
	*prev = lex;
    } else {
	lex->next = lexInput;
	lexInput = lex;
    }
    EXIT ();
}

#ifdef HAVE_LIBREADLINE

volatile int stdin_in_readline;

static void
my_prep_terminal(int meta_flag)
{
    stdin_in_readline = 1;
    rl_prep_terminal(meta_flag);
}

static void
my_deprep_terminal(void)
{
    rl_deprep_terminal();
    stdin_in_readline = 0;
}

static int
LexGetChar (void);

static int
ReadlineGetChar (FILE *f)
{
    return LexGetChar ();
}
#endif

ReferencePtr	LexInputReference;

void
LexInit (void)
{
    ENTER ();

#if HAVE_LIBREADLINE
    rl_getc_function = ReadlineGetChar;
    rl_prep_term_function = my_prep_terminal;
    rl_deprep_term_function = my_deprep_terminal;
    rl_catch_signals = 0;
#endif
    LexInputReference = NewReference ((void **) &lexInput);
    MemAddRoot (LexInputReference);
    EXIT ();
}

Atom
LexFileName (void)
{
    if (lexInput)
	return lexInput->name;
    return AtomId ("<initialization>");
}

int
LexFileLine (void)
{
    if (lexInput)
	return lexInput->lineno;
    return 0;
}

Bool
LexInteractive (void)
{
    if (lexInput)
	return lexInput->interactive;
    return False;
}

Bool
LexResetInteractive (void)
{
    while (lexInput->next && !lexInput->interactive)
    {
	FileClose (lexInput->file);
	lexInput = lexInput->next;
    }
    if (lexInput->interactive)
	return True;
    return False;
}

Bool
LexFile (char *s, Bool complain, Bool after)
{
    Value	f;
    int		err;

    f = FileFopen (s, "r", &err);
    if (f == 0) {
	if (complain)
	    (void) FilePrintf (FileStderr, "%s: %s\n",
			       s, StringChars (&FileGetErrorMessage (err)->string));
	return False;
    }
    (void) NewLexInput(f, AtomId (s), after, False);
    return True;
}

static int
LexGetChar (void)
{
    int	    c;

    for (;;)
    {
	c = FileInput (lexInput->file);
	if (c >= 0)
	    return c;
	if (c == FileBlocked)
	    ThreadsRun (0, lexInput->file);
	else
	{
	    if (!lexInput->next)
		return FileEOF;
	    lexInput->at_eof = True;
	    return '\n';
	}
    }
}

static Value
prompt (void)
{
    Value	v;
    
    if (ignorenl)
	v = lookupVar (0, "prompt2");
    else if (CurrentFrame)
	v = lookupVar (0, "prompt3");
    else
	v = lookupVar (0, "prompt");
    return v;
}

static int
LexGetInteractiveChar (void)
{
#ifdef HAVE_LIBREADLINE
    static char	*line, *line_base;
    int	    c;

    if (!line)
    {
	char    *p;

	p = StrzPart (prompt (), "invalid prompt");
	if (!p)
	    p = "??? ";
	for (;;)
	{
	    FileFlush (FileStdout, False);
	    FileFlush (FileStderr, False);
	    if (FileStdout->file.flags & FileOutputBlocked)
		ThreadsRun (0, FileStdout);
	    else if (FileStderr->file.flags & FileOutputBlocked)
		ThreadsRun (0, FileStderr);
	    else
		break;
	}
	rl_inhibit_completion = 1;
	line_base = readline (p);
	line = line_base;
	if (!line)
	    return FileEOF;
	add_history (line_base);
    }
    c = (*line++) & 0xff;
    if (!c)
    {
	c = '\n';
	free (line_base);
	line = 0;
    }
    return c;
#else
    if (lexInput->at_bol)
    {
	Value	v = prompt ();
	Print (FileStdout, v, 's', 0, 0, 0, ' ');
	FileFlush (FileStdout, True);
    }
    return LexGetChar ();
#endif
}

#undef YY_INPUT
#define YY_NO_UNPUT

static int yy_input (char *buf, int max_size)
{
    int	    c;
    int	    result = 0;
    
    while (result < max_size) {
	if (lexInput->at_eof)
	{
	    FileClose (lexInput->file);
	    lexInput = lexInput->next;
	}
	if (lexInput->interactive)
	    c = LexGetInteractiveChar ();
	else
	    c = LexGetChar ();
	if (lexInput->at_bol) { lexInput->lineno++; }
	lexInput->at_bol = False;
	if (c < 0) 
	    break;
	buf[result++] = c;
	if (c == '\n')
	{
	    lexInput->at_bol = True;
	    break;
	}
    }
    return result;
}

#define YY_INPUT(buf,result,max_size) ((result) = yy_input (buf, max_size))
    
#ifndef FLEX_SCANNER
#undef		input
#undef		unput
int input (void)
{
    char    buf[1];
    int	    r;

    YY_INPUT(buf, r, 1);
    if (r == 0)
	return 0;
    return buf[0];
}

void unput (char c)
{
    if (c == '\n')
	lexInput->lineno--;
    FileUnput (lexInput->file, c);
}
#endif

%}
%%
"/\052"		{   
		    if (seeComment) 
		    {
			yylval.value = lexdoc ();
			return COMMENT_CONST;
		    }
		    else
			skipcomment (); 
		}
^[ \t]*#	skipline();
auto		{ yylval.class = class_auto; return AUTO; }
const		{ yylval.class = class_const; return CONST; }
global		{ yylval.class = class_global; return GLOBAL; }
static		{ yylval.class = class_static; return STATIC; }
function	{ yylval.type = typePrim[rep_void]; return FUNCTION; }
while		{ yylval.ints = WHILE; return WHILE; }
for		{ yylval.ints = FOR; return FOR; }
do		{ yylval.ints = DO; return DO; }
if		{ yylval.ints = IF; return IF; }
else		{ yylval.ints = ELSE; return ELSE; }
switch		{ yylval.ints = SWITCH; return SWITCH; }
break		{ yylval.ints = BREAK; return BREAK; }
continue	{ yylval.ints = CONTINUE; return CONTINUE; }
case		{ yylval.ints = CASE; return CASE; }
default		{ yylval.ints = DEFAULT; return DEFAULT; }
return		{ yylval.ints = RETURNTOK; return RETURNTOK; }
try		{ yylval.ints = TRY; return TRY; }
catch		{ yylval.ints = CATCH; return CATCH; }
twixt		{ yylval.ints = TWIXT; return TWIXT; }

poly		{ yylval.type = typePoly; return POLY; }
bool		{ yylval.type = typePrim[rep_bool]; return BOOL; }
int		{ yylval.type = typePrim[rep_integer]; return INTEGER; }
rational    	{ yylval.type = typePrim[rep_rational]; return RATIONAL; }
real		{ yylval.type = typePrim[rep_float]; return REAL; }
string		{ yylval.type = typePrim[rep_string]; return STRING; }
file		{ yylval.type = typePrim[rep_file]; return FILET; }
semaphore	{ yylval.type = typePrim[rep_semaphore]; return SEMAPHORE; }
continuation	{ yylval.type = typePrim[rep_continuation]; return CONTINUATION; }
thread		{ yylval.type = typePrim[rep_thread]; return THREAD; }
struct		{ yylval.ints = STRUCT; return STRUCT; }
union		{ yylval.ints = UNION; return UNION; }
enum		{ yylval.ints = ENUM; return ENUM; }
void		{ yylval.type = typePrim[rep_void]; return VOID; }
foreign		{ yylval.type = typePrim[rep_foreign]; return FOREIGN; }
true		{ yylval.value = TrueVal; return BOOLVAL; }
false		{ yylval.value = FalseVal; return BOOLVAL; }

typedef		{ yylval.ints = TYPEDEF; return TYPEDEF; }
func		{ yylval.ints = FUNC; return FUNC; }
fork		{ yylval.ints = FORK; return FORK; }
namespace    	{ yylval.ints = NAMESPACE; return NAMESPACE; }
import		{ yylval.ints = IMPORT; return IMPORT; }
exception	{ yylval.ints = EXCEPTION; return EXCEPTION; }
raise		{ yylval.ints = RAISE; return RAISE; }
protected	{ yylval.publish = publish_protected; return PROTECTED; }
public		{ yylval.publish = publish_public; return PUBLIC; }
extend		{ yylval.publish = publish_extend; return EXTEND; }
is_type		{ yylval.ints = ISTYPE; return ISTYPE; }
has_member	{ yylval.ints = HASMEMBER; return HASMEMBER; }
";"		{ yylval.ints = SEMI; return SEMI; }
","		{ yylval.ints = COMMA; return COMMA; }
"$"		{ yylval.ints = DOLLAR; return DOLLAR; }
"..."		{ yylval.ints = DOTDOTDOT; return DOTDOTDOT; }
"…"		{ yylval.ints = DOTDOTDOT; return DOTDOTDOT; }
"."		{ yylval.ints = DOT; return DOT; }
"->"		{ yylval.ints = ARROW; return ARROW; }
"→"		{ yylval.ints = ARROW; return ARROW; }
"=>"		{ yylval.ints = DARROW; return DARROW; }
"⇒"		{ yylval.ints = DARROW; return DARROW; }
"<>"		{ yylval.value = Void; return VOIDVAL; }
"◊"		{ yylval.value = Void; return VOIDVAL; }
\n		{ 
		    if (lexInput->at_eof) { strcpy (yytext, "EOF"); yylval.ints = ENDFILE; return ENDFILE; }
		    if (!ignorenl) { yylval.ints = NL; return NL; } 
		}
"("		{ yylval.ints = OP; ++ignorenl; return OP; }
")"		{ yylval.ints = CP; if (ignorenl > 0) --ignorenl; return CP; }
"*["		{ yylval.ints = STAROS; ++ignorenl; return STAROS; }
"["		{ yylval.ints = OS; ++ignorenl; return OS; }
"]"		{ yylval.ints = CS; if (ignorenl > 0) --ignorenl; return CS; }
"{"		{ yylval.ints = OC; ++ignorenl; return OC; }
"}"		{ yylval.ints = CC; if (ignorenl > 0) --ignorenl; return CC; }
"+="		{ yylval.ints = ASSIGNPLUS; return ASSIGNPLUS; }
"-="		{ yylval.ints = ASSIGNMINUS; return ASSIGNMINUS; }
"*="		{ yylval.ints = ASSIGNTIMES; return ASSIGNTIMES; }
"×="		{ yylval.ints = ASSIGNTIMES; return ASSIGNTIMES; }
"/="		{ yylval.ints = ASSIGNDIVIDE; return ASSIGNDIVIDE; }
"÷="		{ yylval.ints = ASSIGNDIVIDE; return ASSIGNDIVIDE; }
"//="		{ yylval.ints = ASSIGNDIV; return ASSIGNDIV; }
"⫽="		{ yylval.ints = ASSIGNDIV; return ASSIGNDIV; }
"%="		{ yylval.ints = ASSIGNMOD; return ASSIGNMOD; }
"**="		{ yylval.ints = ASSIGNPOW; return ASSIGNPOW; }
"↑="		{ yylval.ints = ASSIGNPOW; return ASSIGNPOW; }
"<<="		{ yylval.ints = ASSIGNSHIFTL; return ASSIGNSHIFTL; }
"⪡="		{ yylval.ints = ASSIGNSHIFTL; return ASSIGNSHIFTL; }
">>="		{ yylval.ints = ASSIGNSHIFTR; return ASSIGNSHIFTR; }
"⪢="		{ yylval.ints = ASSIGNSHIFTR; return ASSIGNSHIFTR; }
"^="		{ yylval.ints = ASSIGNLXOR; return ASSIGNLXOR; }
"&&="           { yylval.ints = ASSIGNAND; return ASSIGNAND; }
"||="           { yylval.ints = ASSIGNOR; return ASSIGNOR; }
"&="		{ yylval.ints = ASSIGNLAND; return ASSIGNLAND; }
"|="		{ yylval.ints = ASSIGNLOR; return ASSIGNLOR; }
"="		{ yylval.ints = ASSIGN; return ASSIGN; }

"+"		{ yylval.ints = PLUS; return PLUS; }
"-"		{ yylval.ints = MINUS; return MINUS; }
"*"		{ yylval.ints = TIMES; return TIMES; }
"×"		{ yylval.ints = TIMES; return TIMES; }
"/"		{ yylval.ints = DIVIDE; return DIVIDE; }
"÷"		{ yylval.ints = DIVIDE; return DIVIDE; }
"//"		{ yylval.ints = DIV; return DIV; }
"⫽"		{ yylval.ints = DIV; return DIV; }
"**"		{ yylval.ints = STARSTAR; return STARSTAR; }
"↑"		{ yylval.ints = STARSTAR; return STARSTAR; }
"%"		{ yylval.ints = MOD; return MOD; }
"!"		{ yylval.ints = BANG; return BANG; }
"#"		{ yylval.ints = POUND; return POUND; }
"&"		{ yylval.ints = LAND; return LAND; }
"|"		{ yylval.ints = LOR; return LOR; }
"^"		{ yylval.ints = LXOR; return LXOR; }
"~"		{ yylval.ints = LNOT; return LNOT; }
"¬"		{ yylval.ints = LNOT; return LNOT; }
"++"		{ yylval.ints = INC; return INC; }
"⧺"		{ yylval.ints = INC; return INC; }
"--"		{ yylval.ints = DEC; return DEC; }
"=="		{ yylval.ints = EQ; return EQ; }
"!="		{ yylval.ints = NE; return NE; }
"≠"		{ yylval.ints = NE; return NE; }
"<"		{ yylval.ints = LT; return LT; }
">"		{ yylval.ints = GT; return GT; }
"<="		{ yylval.ints = LE; return LE; }
"≤"		{ yylval.ints = LE; return LE; }
">="		{ yylval.ints = GE; return GE; }
"≥"		{ yylval.ints = GE; return GE; }
"&&"		{ yylval.ints = AND; return AND; }
"∧"		{ yylval.ints = AND; return AND; }
"||"		{ yylval.ints = OR; return OR; }
"∨"		{ yylval.ints = OR; return OR; }
"<<"		{ yylval.ints = SHIFTL; return SHIFTL; }
"⪡"		{ yylval.ints = SHIFTL; return SHIFTL; }
">>"		{ yylval.ints = SHIFTR; return SHIFTR; }
"⪢"		{ yylval.ints = SHIFTR; return SHIFTR; }
"?"		{ yylval.ints = QUEST; return QUEST; }
"::"		{ yylval.ints = COLONCOLON; return COLONCOLON; }
":"		{ yylval.ints = COLON; return COLON; }
"²"		{ yylval.ints = POW2; return POW2; }
"³"		{ yylval.ints = POW3; return POW3; }
" "		;
"\t"		;
"\r"		;
\'([^\n\']|\\\')*\'	{
			ENTER ();
			char	    *s;
			long	    len = yyleng - 1;
			unsigned    c;

			s = yytext + 1;
			s = StringNextChar (s, &c, &len);
			if (c == '\\')
			{
			    if (!(s = StringNextChar (s, &c, &len)))
				c = 0;
			    else if ('0' <= c && c <= '7')
			    {
				unsigned    ch;
				char	    *ps = s;

				c = c - '0';
				while ((s = StringNextChar (s, &ch, &len)) &&
				       '0' <= ch && ch <= '7')
				{
				    c = (c << 3) | (ch - '0');
				    ps = s;
				}
				s = ps;
			    }
			    else
				c = lexEscape (c);
			}
			yylval.value = NewInt (c);
			EXIT ();
			REFERENCE (yylval.value);
			return CHAR_CONST;
		}
\"([^\n\"]|\\\")*\"	{
			ENTER ();
			char	    *d, *s;
			unsigned    c;
			long	    len = yyleng - 2;

			yylval.value = NewString (len);
			d = StringChars (&yylval.value->string);
			s = yytext + 1;
			while ((s = StringNextChar (s, &c, &len)))
			{
			    if (c == '\\')
			    {
				if (!(s = StringNextChar (s, &c, &len)))
				    break;
				if ('0' <= c && c <= '7')
				{
				    unsigned	ch;
    				    char	*ps = s;
				    long	plen = len;

				    c = c - '0';
				    while ((s = StringNextChar (s, &ch, &len)) &&
					   '0' <= ch && ch <= '7')
				    {
					c = (c << 3) | (ch - '0');
					ps = s;
					plen = len;
				    }
				    s = ps;
				    len = plen;
				}
				else
				    c = lexEscape (c);
			    }
			    d += StringPutChar (c, d);
			}
			*d = '\0';
			yylval.value->string.length = d - StringChars(&yylval.value->string);
			EXIT ();
			REFERENCE (yylval.value);
			return STRING_CONST;
		}
0		{
		yylval.value = Zero;
		return TEN_NUM;
		}
0[0-7_]+	{
		yylval.value = atov(yytext+1, 8);
		return OCTAL0_NUM;
		}
0o[0-7_]+	{
		yylval.value = atov(yytext+2, 8);
		return OCTAL_NUM;
		}
0o[0-7_]+\./\.\.\.   {
		yylval.value = aetov(yytext+2, 8);
		return OCTAL_FLOAT;
		}
0o[0-7_]+/\.\.	{
		yylval.value = atov(yytext+2, 8);
		return OCTAL_NUM;
		}
0o(([0-7_]+((\.[0-7_]*(\{[0-7_]+\})?)?))|(\.[0-7_]+)|(\.[0-7_]*\{[0-7_]+\}))(([Ee][-+]?[0-7_]+)?) {
		yylval.value = aetov (yytext+2, 8);
		return OCTAL_FLOAT;
		}
0b[01_]+		{
		yylval.value = atov(yytext+2, 2);
		return BINARY_NUM;
		}
0b[01_]+\./\.\.\.   {
		yylval.value = aetov(yytext+2, 2);
		return BINARY_FLOAT;
		}
0b[01_]+/\.\.	{
		yylval.value = atov(yytext+2, 2);
		return BINARY_NUM;
		}
0b(([01_]+((\.[01_]*(\{[01_]+\})?)?))|(\.[01_]+)|(\.[01_]*\{[01_]+\}))(([Ee][-+]?[01_]+)?) {
		yylval.value = aetov (yytext+2, 2);
		return BINARY_FLOAT;
		}
0x[0-9a-fA-F_]+	{
		yylval.value = atov(yytext+2, 16);
		return HEX_NUM;
		}
0x[0-9a-fA-F_]+\./\.\.\.   {
		yylval.value = aetov(yytext+2, 16);
		return HEX_FLOAT;
		}
0x[0-9a-fA-F_]+/\.\.	{
		yylval.value = atov(yytext+2, 16);
		return HEX_NUM;
		}
0x(([0-9a-fA-F_]+((\.[0-9a-fA-F_]*(\{[0-9a-fA-F_]+\})?)?))|(\.[0-9a-fA-F_]+)|(\.[0-9a-fA-F_]*\{[0-9a-fA-F_]+\}))(([Ee][-+]?[0-9a-fA-F_]+)?) {
		yylval.value = aetov (yytext+2, 16);
		return HEX_FLOAT;
		}
[0-9][0-9_]*		{
		yylval.value = atov(yytext, 10);
		return TEN_NUM;
		}
[0-9][0-9_]*\./\.\.\.	{
		yylval.value = aetov(yytext, 10);
		return TEN_FLOAT;
		}
[0-9][0-9_]*/\.\.	{
		yylval.value = atov(yytext, 10);
		return TEN_NUM;
		}
(([0-9][0-9_]*((\.[0-9_]*(\{[0-9_]+\})?)?))|(\.[0-9][0-9_]*)|(\.([0-9][0-9_]*)?\{[0-9_]+\}))(([Ee][-+]?[0-9_]+)?) {
		yylval.value = aetov (yytext, 10);
		return TEN_FLOAT;
		}
[a-zA-Z\200-\377_][0-9a-zA-Z\200-\377_]* {
	        CommandPtr	c;
		SymbolPtr	symbol;
		yylval.atom = AtomId (yytext);
		if (!notCommand && (c = CommandFind (CurrentCommands, yylval.atom)))
		{
		    if (c->names)
			return NAMECOMMAND;
		    return COMMAND;
		}
		if (LexNamespace)
		    symbol = NamespaceFindName (LexNamespace, yylval.atom, False);
	        else
		    symbol = NamespaceFindName (CurrentNamespace, yylval.atom, True);
		if (symbol)
		{
		    switch (symbol->symbol.class) {
		    case class_namespace:
			return NAMESPACENAME;
		    case class_typedef:
			return TYPENAME;
		    default:
			break;
		    }
		}
		return NAME;
		}
.		FilePrintf (FileStderr, "character \\%o ignored\n", *yytext & 0xff);
%%

int
lexEscape (int c)
{
    switch (c) {
    case '0':
	return '\0';
	break;
    case 'b':
	return '\b';
	break;
    case 'n':
	return '\n';
	break;
    case 'r':
	return '\r';
	break;
    case 't':
	return '\t';
	break;
    case 'f':
	return '\f';
	break;
    case 'v':
	return '\v';
	break;
    default:
	return c;
    }
}


Value
lexdoc (void)
{
    int	    c;
    Value   s = NewStrString ("");

    c = input();
    if (lexInput->at_eof)
    {
bail:	
        yyerror ("Missing */ at end of file");
        return Void;
    }
    for (;;) {
	Bool skip = False;
	while (c != EOF && c != '*')
	{
	    if (skip && c != ' ' && c != '\t')
		skip = False;
	    if (!skip)
		s = Plus (s, NewCharString (c));
	    if (c == '\n')
		skip = True;
	    c = input();
	    if (c == EOF || lexInput->at_eof)
		goto bail;
	}
	c = input();
	if (c == EOF || lexInput->at_eof)
	    goto bail;
	if (c == '/')
	    break;
	if (!skip)
	    s = Plus (s, NewCharString ('*'));
    }
    return s;
}

void
skipcomment (void)
{
    int	c;

    c = input();
    if (lexInput->at_eof)
    {
bail:	
        yyerror ("Missing */ at end of file");
        return;
    }
    for (;;) {
	while (c != EOF && c != '*')
	{
	    c = input();
	    if (c == EOF || lexInput->at_eof)
		goto bail;
	}
	c = input();
	if (c == EOF || lexInput->at_eof)
	    goto bail;
	if (c == '/')
	    return;
    }
}

void
skipline (void)
{
    int	    c;

    do {
	c = input();
    } while (c != EOF && c != '\n');
}

Value
atov (char *s, int base)
{
    ENTER ();
    Value	result;
    Value	b;
    char	c;
    int		i;

    b = NewInt (base);
    result = NewInt (0);
    for (;;) {
	c = *s++;
	if ('0' <= c  && c <= '9')
	    i = c - '0';
	else if ('a' <= c && c <= 'z')
	    i = c - 'a' + 10;
	else if ('A' <= c && c <= 'Z')
	    i = c - 'A' + 10;
	else if (c == '_')
	    continue;
	else
	    break;
	if (i >= base)
	    break;
	result = Plus (NewInt (i), Times (result, b));
    }
    RETURN (result);
}

static char *
scan_number(char *s, char needle, int *lenp)
{
    int len = 0;
    for (;;) {
	char c = *s;
	if (tolower(c) == needle)
	    break;
	switch (c) {
	case '\0':
	    return NULL;
	case '_':
	    break;
	default:
	    len++;
	    break;
	}
	s++;
    }
    if (lenp)
	*lenp = len;
    return s;
}

static int
number_len(char *s)
{
    int len = 0;
    char c;
    while ((c = *s++))
	if (c != '_')
	    len++;
    return len;
}

Value
aetov (char *s, int base)
{
    ENTER ();
    char    *int_part, *frac_part, *rep_part, *exp_part, *next;
    int	    sign, int_len = -1, frac_len = -1, rep_len = -1, exp_frac_len = -1, esign;
    Value   v, sv;

    int_part = s;
    sign = 1;
    if (*int_part == '+')
	int_part++;
    else if (*int_part == '-') {
	int_part++;
	sign = -1;
    }
    next = int_part;
    frac_part = scan_number(next, '.', &int_len);
    frac_len = -1;
    rep_part = 0;
    rep_len = 0;
    esign = 1;
    if (frac_part) {
	frac_part++;
	next = frac_part;
	rep_part = scan_number(next, '{', &frac_len);
	if (rep_part)
	{
	    rep_part++;
	    next = rep_part;
	    next = scan_number(next, '}', &rep_len);
	    if (!next)
		RETURN(Void);	/* can't happen */
	}
    }
    exp_part = scan_number(next, 'e', &exp_frac_len);
    if (exp_part) {
	if (frac_len < 0)
	    frac_len = exp_frac_len;
	exp_part++;
	if (*exp_part == '+')
	    exp_part++;
	else if (*exp_part == '-') {
	    esign = -1;
	    exp_part++;
	}
    } else if (frac_len < 0 && frac_part)
	frac_len = number_len(frac_part);
    v = atov (int_part, base);
    if (frac_part)
    {
	v = Plus (v, Divide (atov (frac_part, base),
				 Pow (NewInt (base),
				       NewInt (frac_len))));
    }
    if (rep_part)
    {
	Value	rep;

	rep = Divide (atov (rep_part, base), Minus (Pow (NewInt (base),
						       NewInt (rep_len)),
						  One));
	if (frac_len)
	    rep = Divide (rep, Pow (NewInt (base),
				    NewInt (frac_len)));
	v = Plus (v, rep);
    }
    if (exp_part)
    {
	sv = Pow (NewInt (base), atov (exp_part, base));
	if (esign > 0)
	    v = Times (v, sv);
	else
	    v = Divide (v, sv);
    }
    if (sign == -1)
	v = Negate (v);
    RETURN (v);
}
