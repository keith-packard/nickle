%{
/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
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
#define	    FILE_END_CHAR	-2
    
static void lexInputMark (void *object)
{
    LexInput *lex = object;
    
    MemReference (lex->next);
    MemReference (lex->file);
}

static DataType lexInputType = {
    lexInputMark, 0
};

static void
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

void
LexStdin (void)
{
    NewLexInput(FileStdin, 0, True, stdin_interactive);
}
	
Bool
LexFile (char *s, Bool complain, Bool after)
{
    Value	f;

    f = FileFopen (s, "r");
    if (f == 0) {
	if (complain)
	    (void) FilePrintf (FileStderr, "cannot open file %s\n", s);
	return False;
    }
    (void) NewLexInput(f, AtomId (s), after, False);
    return True;
}

Bool
LexLibrary (char *filename, Bool complain, Bool after)
{
    ENTER ();
    Value   library_path;
    char    *nicklelib;
    char    *lib;
    char    *colon;
    Bool    found = False;

    library_path = lookupVar ("Command", "library_path");
    if (library_path == Zero || library_path->value.tag != type_string)
    {
	if (complain)
	    (void) FilePrintf (FileStderr, "Command::library_path not set\n");
	EXIT ();
	return False;
    }
    lib = StringChars (&library_path->string);
    while (*lib)
    {
	colon = strchr (lib, ':');
	if (!colon)
	    colon = lib + strlen (lib);
	nicklelib = AllocateTemp (colon - lib + strlen (filename) + 2);
	strncpy (nicklelib, lib, colon-lib);
	nicklelib[colon-lib] = '\0';
	strcat (nicklelib, "/");
	strcat (nicklelib, filename);
        if (LexFile (nicklelib, False, after))
	{
	    found = True;
	    break;
	}
	lib = colon;
	if (*lib == ':')
	    lib++;
    }
    if (complain && !found)
	(void) FilePrintf (FileStderr, "cannot locate library %s\n", filename);
    EXIT ();
    return found;
}

void
LexString (char *s, Bool after)
{
    Value   f;

    f = FileStringRead (s, strlen (s));
    (void) NewLexInput (f, 0, after, False);
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
	if (v && v->value.tag == type_string)
	    p = StringChars (&v->string);
	else
	    p = "??? ";
	FileFlush (FileStdout);
	rl_getc_function = ReadlineGetChar;
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
	dofformat (FileStdout, "%s", 1, &v);
	FileFlush (FileStdout);
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
^#		skipline();
auto		{ yylval.class = class_auto; return AUTO; }
global		{ yylval.class = class_global; return GLOBAL; }
static		{ yylval.class = class_static; return STATIC; }
function	{ yylval.type = type_func; return FUNCTION; }
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

poly		{ yylval.type = type_undef; return POLY; }
int		{ yylval.type = type_integer; return INTEGER; }
rational    	{ yylval.type = type_rational; return RATIONAL; }
real		{ yylval.type = type_float; return REAL; }
string		{ yylval.type = type_string; return STRING; }
file		{ yylval.type = type_file; return FILET; }
semaphore	{ yylval.type = type_semaphore; return SEMAPHORE; }
continuation	{ yylval.type = type_continuation; return CONTINUATION; }
thread		{ yylval.type = type_thread; return THREAD; }
struct		{ yylval.ints = STRUCT; return STRUCT; }
union		{ yylval.ints = UNION; return UNION; }
void		{ yylval.type = type_void; return VOID; }

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
\'\\?.\'	{   ENTER ();
		    int	i;
		    
		    if (yytext[1] == '\\')
			i = lexEscape (yytext[2]);
		    else
			i = yytext[1];
		    yylval.value = NewInt (i);
		    EXIT ();
		    REFERENCE (yylval.value);
		    return CHAR_CONST;
		}
\"([^\n\"]|\\\")*\"	{
			ENTER ();
			register char	*c, *s;
    			yytext[yyleng - 1] = '\0';
			yylval.value = NewString (yyleng-2);
			c = StringChars (&yylval.value->string);
			s = yytext + 1;
			while (*s) {
				if (*s == '\\')
					*c++ = lexEscape (*++s);
				else
					*c++ = *s;
				++s;
			}
			*c = '\0';
			EXIT ();
			REFERENCE (yylval.value);
			return STRING_CONST;
		}
0[0-7]*		{
		yylval.value = atov(yytext+1, 8);
		return OCTAL_CONST;
		}
0b[01]+		{
		yylval.value = atov(yytext+2, 2);
		return BINARY_CONST;
		}
0x[0-9a-fA-F]+	{
		yylval.value = atov(yytext+2, 16);
		return HEX_CONST;
		}
[0-9]+		{
		yylval.value = atov(yytext, 10);
		return TEN_CONST;
		}
[0-9]+\./\.\.\.	{
		yylval.value = aetov(yytext);
		return FLOAT_CONST;
		}
[0-9]+/\.\.	{
		yylval.value = atov(yytext, 10);
		return TEN_CONST;
		}
(([0-9]+((\.[0-9]*(\{[0-9]+\})?)?))|(\.[0-9]+)|(\.[0-9]*\{[0-9]+\}))(([Ee][-+]?[0-9]+)?) {
		yylval.value = aetov (yytext);
		return FLOAT_CONST;
		}
[a-zA-Z][0-9a-zA-Z_]* {
	        CommandPtr	c;
		SymbolPtr	symbol;
		yylval.atom = AtomId (yytext);
		if ((c = CommandFind (CurrentCommands, yylval.atom)))
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
    Value	d, b;

    b = NewInt (base);
    result = NewInt (0);
    for (;;) {
	switch (*s) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	    d = NewInt (*s - '0');
	    break;
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	    d = NewInt (*s - 'a' + 10);
	    break;
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
	    d = NewInt (*s - 'A' + 10);
	    break;
	default:
	    RETURN (result);
	}
	if (d->ints.value >= base)
	    break;
	result = Plus (d, Times (result, b));
	++s;
    }
    RETURN (result);
}

Value
aetov (char *s)
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
		RETURN (Zero);	    /* "can't" happen */
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
    v = atov (int_part, 10);
    if (frac_part)
    {
	v = Plus (v, Divide (atov (frac_part, 10),
				 Pow (NewInt (10),
				       NewInt (frac_len))));
    }
    if (rep_part)
    {
	Value	rep;
	
	rep = Divide (atov (rep_part, 10), Minus (Pow (NewInt (10),
						       NewInt (rep_len)),
						  One));
	if (frac_len)
	    rep = Divide (rep, Pow (NewInt (10),
				    NewInt (frac_len)));
	v = Plus (v, rep);
    }
    if (exp_part) 
    {
	sv = Pow (NewInt (10), atov (exp_part, 10));
	if (esign > 0)
	    v = Times (v, sv);
	else
	    v = Divide (v, sv);
    }
    RETURN (v);
}

