%{
/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"nickle.h"
#include	"gram.h"
#include	<strings.h>
#include	<errno.h>

int	    at_bol = True;
int	    yylineno;
    
#undef YY_INPUT

#define YY_INPUT(buf,result,max_size) \
{ \
    int c; \
    result = 0; \
    if (at_bol) yyprompt (); \
    while (result < max_size) { \
	c = FileInput (yyinput); \
	at_bol = False; \
	if (c < 0) { \
	    c = popinput (); \
	    if (c < 0) \
		break; \
	} \
	buf[result++] = c; \
	if (c == '\n') \
	{ \
	    at_bol = True; \
	    yylineno++; \
	    break; \
	} \
    } \
}
    
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
	yylineno--;
    FileUnput (yyinput, c);
}
#endif

#define		YYINDEEP	20
Value	yyinstack[YYINDEEP];
Value	*yyinpt = yyinstack + YYINDEEP;
Value	yyinput;
int		linenos[YYINDEEP];
int		*linenopt = linenos + YYINDEEP;
char		*yyfile = "";
char		*fnames[YYINDEEP];
char		**fnamept = fnames + YYINDEEP;
int		yyfiledeep = 0;

%}
%%
"/\052"		skipcomment();
^#		skipline();
auto		{ yylval.cval = class_auto; return AUTO; }
global		{ yylval.cval = class_global; return GLOBAL; }
static		{ yylval.cval = class_static; return STATIC; }
function	{ yylval.tval = type_func; return FUNCTION; }
undefine	{ return UNDEFINE; }
quit		return QUIT;
exit		return QUIT;
edit		return EDIT;
history		return HISTORY;
print		return PRINT;
read		return READ;
while		{ yylval.ival = WHILE; return WHILE; }
for		{ yylval.ival = FOR; return FOR; }
do		{ yylval.ival = DO; return DO; }
if		{ yylval.ival = IF; return IF; }
else		{ yylval.ival = ELSE; return ELSE; }
break		{ yylval.ival = BREAK; return BREAK; }
continue	{ yylval.ival = CONTINUE; return CONTINUE; }
return		{ yylval.ival = RETURNTOK; return RETURNTOK; }
try		{ yylval.ival = TRY; return TRY; }
catch		{ yylval.ival = CATCH; return CATCH; }

poly		{ yylval.tval = type_undef; return POLY; }
int		{ yylval.tval = type_int; return INT; }
integer		{ yylval.tval = type_integer; return INTEGER; }
rational    	{ yylval.tval = type_rational; return RATIONAL; }
real		{ yylval.tval = type_float; return REAL; }
string		{ yylval.tval = type_string; return STRING; }
File		{ yylval.tval = type_file; return FILET; }
Mutex		{ yylval.tval = type_mutex; return MUTEX; }
Semaphore	{ yylval.tval = type_semaphore; return SEMAPHORE; }
Continuation	{ yylval.tval = type_continuation; return CONTINUATION; }

typedef		{ yylval.ival = TYPEDEF; return TYPEDEF; }
new		{ yylval.ival = NEW; return NEW; }
func		{ yylval.ival = FUNC; return FUNC; }
fork		{ yylval.ival = FORK; return FORK; }
scope		{ yylval.ival = SCOPE; return SCOPE; }
import		{ yylval.ival = IMPORT; return IMPORT; }
exception	{ yylval.ival = EXCEPTION; return EXCEPTION; }
public		{ yylval.pval = publish_public; return PUBLIC; }
";"		{ yylval.ival = SEMI; return SEMI; }
","		{ yylval.ival = COMMA; return COMMA; }
"$"		{ yylval.ival = DOLLAR; return DOLLAR; }
"."		{ yylval.ival = DOT; return DOT; }
"->"		{ yylval.ival = ARROW; return ARROW; }
"*["		{ yylval.ival = REFARRAY; return REFARRAY; }
\n		{ if (!ignorenl) { yylval.ival = NL; return NL; } }
"("		{ yylval.ival = OP; return OP; }
")"		{ yylval.ival = CP; return CP; }
"["		{ yylval.ival = OS; return OS; }
"]"		{ yylval.ival = CS; return CS; }
"{"		{ yylval.ival = OC; return OC; }
"}"		{ yylval.ival = CC; return CC; }
"+"		{ yylval.ival = PLUS; return PLUS; }
"-"		{ yylval.ival = MINUS; return MINUS; }
"*"		{ yylval.ival = TIMES; return TIMES; }
"/"		{ yylval.ival = DIVIDE; return DIVIDE; }
"//"		{ yylval.ival = DIV; return DIV; }
"%"		{ yylval.ival = MOD; return MOD; }
"!"		{ yylval.ival = BANG; return BANG; }
"#"		{ yylval.ival = POUND; return POUND; }
"^"		{ yylval.ival = POW; return POW; }
"&"		{ yylval.ival = LAND; return LAND; }
"|"		{ yylval.ival = LOR; return LOR; }
"^^"		{ yylval.ival = LXOR; return LXOR; }
"~"		{ yylval.ival = LNOT; return LNOT; }
"="		{ yylval.ival = ASSIGN; return ASSIGN; }
"++"		{ yylval.ival = INC; return INC; }
"--"		{ yylval.ival = DEC; return DEC; }
"=="		{ yylval.ival = EQ; return EQ; }
"!="		{ yylval.ival = NE; return NE; }
"<"		{ yylval.ival = LT; return LT; }
">"		{ yylval.ival = GT; return GT; }
"<="		{ yylval.ival = LE; return LE; }
">="		{ yylval.ival = GE; return GE; }
"&&"		{ yylval.ival = AND; return AND; }
"||"		{ yylval.ival = OR; return OR; }
"?"		{ yylval.ival = QUEST; return QUEST; }
":"		{ yylval.ival = COLON; return COLON; }
" "		;
"\t"		;
\'\\?.\'	{   ENTER ();
		    int	i;
		    
		    if (yytext[1] == '\\')
			i = lexEscape (yytext[2]);
		    else
			i = yytext[1];
		    yylval.vval = NewInt (i);
		    EXIT ();
		    REFERENCE (yylval.vval);
		    return CONST;
		}
\"([^\n\"]|\\\")*\"	{
			ENTER ();
			register char	*c, *s;
    			yytext[yyleng - 1] = '\0';
			yylval.vval = NewString (yyleng-2);
			c = StringChars (&yylval.vval->string);
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
			REFERENCE (yylval.vval);
			return CONST;
		}
0[0-7]*		{
		yylval.vval = atov(yytext+1, 8);
		return CONST;
		}
0b[01]+		{
		yylval.vval = atov(yytext+2, 2);
		return CONST;
		}
0x[0-9a-fA-F]+	{
		yylval.vval = atov(yytext+2, 16);
		return CONST;
		}
[0-9]+		{
		yylval.vval = atov(yytext, 10);
		return CONST;
		}
(([0-9]+((\.[0-9]*(\{[0-9]+\})?)?))|(\.[0-9]+)|(\.[0-9]*\{[0-9]+\}))(([Ee][-+]?[0-9]+)?) {
		yylval.vval = aetov (yytext);
		return CONST;
		}
[a-zA-Z][0-9a-zA-Z_]* {
		yylval.aval = AtomId (yytext);
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
    for (;;) {
	while (c != EOF && c != '*')
	    c = input();
	if (c == EOF)
	    return;
	c = input();
	if (c == EOF || c == '/')
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
    } else if (frac_len < 0)
	frac_len = strlen(frac_part);
    v = atov (int_part, 10);
    if (frac_part && frac_len)
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

Bool
lexfile(char *s)
{
    Value	f;
    f = FileFopen (s, "r");
    interactive = 0;
    if (f == 0) {
	FilePuts (FileStderr, "cannot open file \"");
	FilePuts (FileStderr, s);
	FilePuts (FileStderr, "\"\n");
	return False;
    }
    ++yyfiledeep;
    yyinput = f;
    yyfile = s;
    return True;
}

void
lexstdin(void)
{
    --yyfiledeep;
    (void) FileClose (yyinput);
    yyinput = FileStdin;
    interactive = stdin_interactive;
}
	
Bool
pushinput (char *s, Bool complain)
{
    Value	f;

    if (yyinpt == yyinstack) {
	(void) FilePrintf (FileStderr, "files nested too deeply\n");
	return False;
    }
    f = FileFopen (s, "r");
    if (f == 0) {
	if (complain)
	    (void) FilePrintf (FileStderr, "cannot open file %s\n", s);
	return False;
    }
    ++yyfiledeep;
    *--yyinpt = yyinput;
    *--linenopt = yylineno;
    *--fnamept = yyfile;
    yyinput = f;
    yylineno = 1;
    yyfile = s;
    interactive = 0;
    return True;
}

int
popinput (void)
{
    int	    c;

/*    printf ("popinput\n"); */
    for (;;)
    {
	c = FileInput (yyinput);
	if (c >= 0)
	    return c;
	if (c == FileBlocked)
	    ThreadsRun (0, yyinput);
	else
	{
	    FileClose (yyinput);
	    if (yyinpt == yyinstack + YYINDEEP)
		return FileEOF;
	    yyinput = *yyinpt++;
	    if (yyinput == FileStdin)
	    {
		interactive = stdin_interactive;
		yyprompt ();
	    }
	    yylineno = *linenopt++;
	    yyfile = *fnamept++;
	    --yyfiledeep;
	}
    }
}

void
yyprompt (void)
{
    ENTER ();
    Value	v;
    
    if (interactive)
    {
	if (ignorenl)
	    v = lookupVar ("prompt2");
	else if (CurrentFrame)
	    v = lookupVar ("prompt3");
	else
	    v = lookupVar ("prompt");
	dofformat (FileStdout, "%s", 1, &v);
	FileFlush (FileStdout);
    }
    EXIT ();
}

