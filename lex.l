%{
/* $Header$ */

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

ReferencePtr	LexInputReference;

void
LexInit (void)
{
    ENTER ();

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
			       s, FileGetErrorMessage (err));
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
	    FileClose (lexInput->file);
	    
	    if (!lexInput->next)
		return FileEOF;
	    lexInput = lexInput->next;
	    lexInput->at_eof = True;
	    return '\n';
	}
    }
}

#ifdef HAVE_LIBREADLINE
static int
ReadlineGetChar (FILE *f)
{
    return LexGetChar ();
}
#endif

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
	Value	v;

	v = prompt ();
	if (v && ValueIsString(v))
	    p = StringChars (&v->string);
	else
	    p = "??? ";
	for (;;)
	{
	    FileFlush (FileStdout, False);
	    if (FileStdout->file.flags & FileOutputBlocked)
		ThreadsRun (0, FileStdout);
	    else
		break;
	}
	rl_getc_function = ReadlineGetChar;
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
    
    lexInput->at_eof = False;
    if (lexInput->at_bol) { lexInput->lineno++; }
    while (result < max_size) {
	if (lexInput->interactive)
	    c = LexGetInteractiveChar ();
	else
	    c = LexGetChar ();
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
"/\052"		skipcomment();
^[ \t]*#	skipline();
auto		{ yylval.class = class_auto; return AUTO; }
const		{ yylval.class = class_const; return CONST; }
global		{ yylval.class = class_global; return GLOBAL; }
static		{ yylval.class = class_static; return STATIC; }
function	{ yylval.type = typePrim[rep_func]; return FUNCTION; }
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
";"		{ yylval.ints = SEMI; return SEMI; }
","		{ yylval.ints = COMMA; return COMMA; }
"$"		{ yylval.ints = DOLLAR; return DOLLAR; }
"..."		{ yylval.ints = DOTS; return DOTS; }
"."		{ yylval.ints = DOT; return DOT; }
"->"		{ yylval.ints = ARROW; return ARROW; }
"=>"		{ yylval.ints = DARROW; return DARROW; }
"<>"		{ yylval.value = Void; return VOIDVAL; }
\n		{ 
		    if (!ignorenl) { yylval.ints = NL; return NL; } 
		}
"("		{ yylval.ints = OP; ++ignorenl; return OP; }
")"		{ yylval.ints = CP; --ignorenl; return CP; }
"*["		{ yylval.ints = STAROS; ++ignorenl; return STAROS; }
"["		{ yylval.ints = OS; ++ignorenl; return OS; }
"]"		{ yylval.ints = CS; --ignorenl; return CS; }
"{"		{ yylval.ints = OC; ++ignorenl; return OC; }
"}"		{ yylval.ints = CC; --ignorenl; return CC; }
"+="		{ yylval.ints = ASSIGNPLUS; return ASSIGNPLUS; }
"-="		{ yylval.ints = ASSIGNMINUS; return ASSIGNMINUS; }
"*="		{ yylval.ints = ASSIGNTIMES; return ASSIGNTIMES; }
"/="		{ yylval.ints = ASSIGNDIVIDE; return ASSIGNDIVIDE; }
"//="		{ yylval.ints = ASSIGNDIV; return ASSIGNDIV; }
"%="		{ yylval.ints = ASSIGNMOD; return ASSIGNMOD; }
"**="		{ yylval.ints = ASSIGNPOW; return ASSIGNPOW; }
"<<="		{ yylval.ints = ASSIGNSHIFTL; return ASSIGNSHIFTL; }
">>="		{ yylval.ints = ASSIGNSHIFTR; return ASSIGNSHIFTR; }
"^="		{ yylval.ints = ASSIGNLXOR; return ASSIGNLXOR; }
"&="		{ yylval.ints = ASSIGNLAND; return ASSIGNLAND; }
"|="		{ yylval.ints = ASSIGNLOR; return ASSIGNLOR; }
"="		{ yylval.ints = ASSIGN; return ASSIGN; }

"+"		{ yylval.ints = PLUS; return PLUS; }
"-"		{ yylval.ints = MINUS; return MINUS; }
"*"		{ yylval.ints = TIMES; return TIMES; }
"/"		{ yylval.ints = DIVIDE; return DIVIDE; }
"//"		{ yylval.ints = DIV; return DIV; }
"**"		{ yylval.ints = POW; return POW; }
"%"		{ yylval.ints = MOD; return MOD; }
"!"		{ yylval.ints = BANG; return BANG; }
"#"		{ yylval.ints = POUND; return POUND; }
"&"		{ yylval.ints = LAND; return LAND; }
"|"		{ yylval.ints = LOR; return LOR; }
"^"		{ yylval.ints = LXOR; return LXOR; }
"~"		{ yylval.ints = LNOT; return LNOT; }
"++"		{ yylval.ints = INC; return INC; }
"--"		{ yylval.ints = DEC; return DEC; }
"=="		{ yylval.ints = EQ; return EQ; }
"!="		{ yylval.ints = NE; return NE; }
"<"		{ yylval.ints = LT; return LT; }
">"		{ yylval.ints = GT; return GT; }
"<="		{ yylval.ints = LE; return LE; }
">="		{ yylval.ints = GE; return GE; }
"&&"		{ yylval.ints = AND; return AND; }
"||"		{ yylval.ints = OR; return OR; }
"<<"		{ yylval.ints = SHIFTL; return SHIFTL; }
">>"		{ yylval.ints = SHIFTR; return SHIFTR; }
"?"		{ yylval.ints = QUEST; return QUEST; }
"::"		{ yylval.ints = COLONCOLON; return COLONCOLON; }
":"		{ yylval.ints = COLON; return COLON; }
" "		;
"\t"		;
\'([^\n\']|\\\')*\'	{
			ENTER ();
			char	 *s;
			int	c;
    			yytext[yyleng - 1] = '\0';
			s = yytext + 1;
			s = StringNextChar (s, &c);
			if (c == '\\')
			{
			    if (!(s = StringNextChar (s, &c)))
				c = 0;
			    else if ('0' <= c && c <= '7')
			    {
				int	ch;
				char    *ps = s;
				c = c - '0';
				while ((s = StringNextChar (s, &ch)) &&
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
			char	*d, *s;
			int	c;
    			yytext[yyleng - 1] = '\0';
			yylval.value = NewString (yyleng-2);
			d = StringChars (&yylval.value->string);
			s = yytext + 1;
			while ((s = StringNextChar (s, &c)))
			{
			    if (c == '\\')
			    {
				if (!(s = StringNextChar (s, &c)))
				    break;
				if ('0' <= c && c <= '7')
				{
				    int	    ch;
    				    char    *ps = s;
				    c = c - '0';
				    while ((s = StringNextChar (s, &ch)) &&
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
			    d += StringPutChar (c, d);
			}
			*d = '\0';
			EXIT ();
			REFERENCE (yylval.value);
			return STRING_CONST;
		}
0[0-7]*		{
		yylval.value = atov(yytext+1, 8);
		if (yytext[1] == '\0')
		    return TEN_NUM;
	        else
		    return OCTAL0_NUM;
		}
0o[0-7]+	{
		yylval.value = atov(yytext+2, 8);
		return OCTAL_NUM;
		}
0o[0-7]+\./\.\.\.   {
		yylval.value = aetov(yytext+2, 8);
		return OCTAL_FLOAT;
		}
0o[0-7]+/\.\.	{
		yylval.value = atov(yytext+2, 8);
		return OCTAL_NUM;
		}
0o(([0-7]+((\.[0-7]*(\{[0-7]+\})?)?))|(\.[0-7]+)|(\.[0-7]*\{[0-7]+\}))(([Ee][-+]?[0-7]+)?) {
		yylval.value = aetov (yytext+2, 8);
		return OCTAL_FLOAT;
		}
0b[01]+		{
		yylval.value = atov(yytext+2, 2);
		return BINARY_NUM;
		}
0b[0-1]+\./\.\.\.   {
		yylval.value = aetov(yytext+2, 2);
		return BINARY_FLOAT;
		}
0b[0-1]+/\.\.	{
		yylval.value = atov(yytext+2, 2);
		return BINARY_NUM;
		}
0b(([0-1]+((\.[0-1]*(\{[0-1]+\})?)?))|(\.[0-1]+)|(\.[0-1]*\{[0-1]+\}))(([Ee][-+]?[0-1]+)?) {
		yylval.value = aetov (yytext+2, 2);
		return BINARY_FLOAT;
		}
0x[0-9a-fA-F]+	{
		yylval.value = atov(yytext+2, 16);
		return HEX_NUM;
		}
0x[0-9a-fA-F]+\./\.\.\.   {
		yylval.value = aetov(yytext+2, 16);
		return HEX_FLOAT;
		}
0x[0-9a-fA-F]+/\.\.	{
		yylval.value = atov(yytext+2, 16);
		return HEX_NUM;
		}
0x(([0-9a-fA-F]+((\.[0-9a-fA-F]*(\{[0-9a-fA-F]+\})?)?))|(\.[0-9a-fA-F]+)|(\.[0-9a-fA-F]*\{[0-9a-fA-F]+\}))(([Ee][-+]?[0-9a-fA-F]+)?) {
		yylval.value = aetov (yytext+2, 16);
		return HEX_FLOAT;
		}
[0-9]+		{
		yylval.value = atov(yytext, 10);
		return TEN_NUM;
		}
[0-9]+\./\.\.\.	{
		yylval.value = aetov(yytext, 10);
		return TEN_FLOAT;
		}
[0-9]+/\.\.	{
		yylval.value = atov(yytext, 10);
		return TEN_NUM;
		}
(([0-9]+((\.[0-9]*(\{[0-9]+\})?)?))|(\.[0-9]+)|(\.[0-9]*\{[0-9]+\}))(([Ee][-+]?[0-9]+)?) {
		yylval.value = aetov (yytext, 10);
		return TEN_FLOAT;
		}
[a-zA-Z][0-9a-zA-Z_]* {
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

void
skipcomment (void)
{
    int	c;

    c = input();
    if (lexInput->at_eof)
    {
bail:	
        yyerror ("Missing */ at end of file");
	lexInput->at_eof = False;
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
	else
	    break;
	if (i >= base)
	    break;
	result = Plus (NewInt (i), Times (result, b));
    }
    RETURN (result);
}

Value
aetov (char *s, int base)
{
    ENTER ();
    char    *int_part, *frac_part, *rep_part, *exp_part, *next;
    int	    sign, frac_len, rep_len, esign;
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
    frac_part = strchr (next, '.');
    frac_len = -1;
    rep_part = 0;
    rep_len = 0;
    esign = 1;
    if (frac_part) {
	frac_part++;
	next = frac_part;
	rep_part = strchr (next, '{');
	if (rep_part)
	{
	    frac_len = rep_part - frac_part;
	    rep_part++;
	    next = strchr (rep_part, '}');
	    if (!next)
		RETURN (Void);	    /* "can't" happen */
	    rep_len = next - rep_part;
	    next = next + 1;
	}
    }
    exp_part = strchr (next, 'e');
    if (!exp_part)
	exp_part = strchr (next, 'E');
    if (exp_part) {
	if (frac_len < 0)
	    frac_len = exp_part - frac_part;
	exp_part++;
	if (*exp_part == '+')
	    exp_part++;
	else if (*exp_part == '-') {
	    esign = -1;
	    exp_part++;
	}
    } else if (frac_len < 0 && frac_part)
	frac_len = strlen(frac_part);
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

