%{
/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	"gram.h"
#include	<strings.h>
#include	<errno.h>

int	    at_bol = True;
int	    yylineno = 0;
int	    at_eof = False;
    
#undef YY_INPUT
#define YY_NO_UNPUT

#define	    FILE_END_CHAR	-2
    
static int yy_input (char *buf, int max_size)
{
    int	    c;
    int	    result = 0;
    
    at_eof = False;
    if (at_bol) { yylineno++; yyprompt (); }
    while (result < max_size) {
	c = FileInput (yyinput);
	at_bol = False;
	if (c < 0) {
	    c = popinput ();
	    if (c == -2) {
		at_eof = True;
		buf[result++] = '\n';
		break;
	    }
	    if (c < 0)
		break;
	}
	buf[result++] = c;
	if (c == '\n')
	{
	    at_bol = True;
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
Atom		yyfile = 0;
Atom		fnames[YYINDEEP];
Atom		*fnamept = fnames + YYINDEEP;
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
load		return LOAD;
while		{ yylval.ival = WHILE; return WHILE; }
for		{ yylval.ival = FOR; return FOR; }
do		{ yylval.ival = DO; return DO; }
if		{ yylval.ival = IF; return IF; }
else		{ yylval.ival = ELSE; return ELSE; }
switch		{ yylval.ival = SWITCH; return SWITCH; }
typeswitch	{ yylval.ival = TYPESWITCH; return TYPESWITCH; }
break		{ yylval.ival = BREAK; return BREAK; }
continue	{ yylval.ival = CONTINUE; return CONTINUE; }
case		{ yylval.ival = CASE; return CASE; }
default		{ yylval.ival = DEFAULT; return DEFAULT; }
return		{ yylval.ival = RETURNTOK; return RETURNTOK; }
try		{ yylval.ival = TRY; return TRY; }
catch		{ yylval.ival = CATCH; return CATCH; }
twixt		{ yylval.ival = TWIXT; return TWIXT; }

poly		{ yylval.tval = type_undef; return POLY; }
integer		{ yylval.tval = type_integer; return INTEGER; }
rational    	{ yylval.tval = type_rational; return RATIONAL; }
real		{ yylval.tval = type_float; return REAL; }
string		{ yylval.tval = type_string; return STRING; }
file		{ yylval.tval = type_file; return FILET; }
mutex		{ yylval.tval = type_mutex; return MUTEX; }
semaphore	{ yylval.tval = type_semaphore; return SEMAPHORE; }
continuation	{ yylval.tval = type_continuation; return CONTINUATION; }
thread		{ yylval.tval = type_thread; return THREAD; }
struct		{ yylval.ival = STRUCT; return STRUCT; }
union		{ yylval.ival = UNION; return UNION; }

typedef		{ yylval.ival = TYPEDEF; return TYPEDEF; }
func		{ yylval.ival = FUNC; return FUNC; }
fork		{ yylval.ival = FORK; return FORK; }
namespace    	{ yylval.ival = NAMESPACE; return NAMESPACE; }
import		{ yylval.ival = IMPORT; return IMPORT; }
exception	{ yylval.ival = EXCEPTION; return EXCEPTION; }
raise		{ yylval.ival = RAISE; return RAISE; }
public		{ yylval.pval = publish_public; return PUBLIC; }
extend		{ yylval.pval = publish_extend; return EXTEND; }
";"		{ yylval.ival = SEMI; return SEMI; }
","		{ yylval.ival = COMMA; return COMMA; }
"$"		{ yylval.ival = DOLLAR; return DOLLAR; }
"..."		{ yylval.ival = DOTS; return DOTS; }
"."		{ yylval.ival = DOT; return DOT; }
"->"		{ yylval.ival = ARROW; return ARROW; }
"*["		{ yylval.ival = REFARRAY; return REFARRAY; }
\n		{ 
		    if (!ignorenl) { yylval.ival = NL; return NL; } 
		}
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
"<<"		{ yylval.ival = SHIFTL; return SHIFTL; }
">>"		{ yylval.ival = SHIFTR; return SHIFTR; }
"?"		{ yylval.ival = QUEST; return QUEST; }
"::"		{ yylval.ival = COLONCOLON; return COLONCOLON; }
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
		if (TypespaceFind (CurrentTypespace, yylval.aval))
		    return TYPENAME;
		else
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
    if (at_eof)
    {
bail:	
        yyerror ("Missing */ at end of file");
	at_eof = False;
        return;
    }
    for (;;) {
	while (c != EOF && c != '*')
	{
	    c = input();
	    if (c == EOF || at_eof)
		goto bail;
	}
	c = input();
	if (c == EOF || at_eof)
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
    yyfile = AtomId (s);
    return True;
}

void
lexstdin(void)
{
    if (yyfiledeep)
    {
	--yyfiledeep;
	(void) FileClose (yyinput);
    }
    yyfile = 0;
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
    yylineno = 0;
    at_bol = True;
    yyfile = AtomId (s);
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
	    return FILE_END_CHAR;
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

