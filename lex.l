%{

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include "nickle.h"
#include "gram.h"
#include "ref.h"
#include <ctype.h>

/* nickle.h declares yylex and yywrap, so keep flex from declaring them again */
#define YY_DECL int yylex(void)
#define YY_SKIP_YYWRAP

/*
 * Tokens store their semantic value through these, which return the
 * token, so lexValue holds yylval.value when yylval holds a Value, and
 * 0 otherwise. lexValue is a root: the parser may hold that token as
 * lookahead after releasing what a command allocated. The input ends
 * without a token, so clear it there too
 */
static Value        lexValue;
static ReferencePtr lexValueReference;

#define yyterminate() return (lexValue = 0, YY_NULL)

/*
 * The brackets open in the input, counted apart from the grammar's
 * parseIgnorenl; newlines are insignificant while either is nonzero. A
 * command closes all of its brackets, so when it ends, those still open
 * belong to the token read ahead
 */
int         lexIgnorenl;

static void lexNoteToken(int token);
static void lexBlockComment(void);
static void lexHashComment(void);
static void lexFallThroughComment(CommentStyle style);

static inline int
yylvalValue(Value value, int token)
{
    yylval.value = value;
    lexValue = value;
    lexNoteToken(token);
    return token;
}

/*
 * A number's value is its text as written, which the parser evaluates
 * (ParseNumberValue) and keeps with the constant so it prints that way
 */
static inline int
yylvalNumber(int token)
{
    return yylvalValue(NewStrString(yytext), token);
}

static inline int
yylvalClass(Class class, int token)
{
    yylval.class = class;
    lexValue = 0;
    lexNoteToken(token);
    return token;
}

static inline int
yylvalType(Type *type, int token)
{
    yylval.type = type;
    lexValue = 0;
    lexNoteToken(token);
    return token;
}

static inline int
yylvalInts(int token)
{
    yylval.ints = token;
    lexValue = 0;
    lexNoteToken(token);
    return token;
}

/* An operator token written in UTF-8, which prints that way (EXPR_UTF8) */
static inline int
yylvalUtf8(int token)
{
    yylval.ints = token | LEX_UTF8;
    lexValue = 0;
    lexNoteToken(token);
    return token;
}

static inline int
yylvalPublish(Publish publish, int token)
{
    yylval.publish = publish;
    lexValue = 0;
    lexNoteToken(token);
    return token;
}

static inline int
yylvalAtom(Atom atom, int token)
{
    yylval.atom = atom;
    lexValue = 0;
    lexNoteToken(token);
    return token;
}

/* The value of the hex digit 'c' */
static uint32_t
lexHexDigit(uint32_t c)
{
    if (c <= '9')
        return c - '0';
    return (c | 0x20) - 'a' + 10;
}

#include <strings.h>
#include <errno.h>
#ifdef HAVE_LIBREADLINE
#ifdef HAVE_LIBEDIT
#include <readline.h>
#else
#include <readline/readline.h>
#include <readline/history.h>
#endif
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

typedef struct _lexInput {
    DataType          data;
    struct _lexInput *next;
    Value             file;
    Atom              name;
    int               lineno;
    Bool              at_bol;
    Bool              at_eof;
    Bool              interactive;
    Bool              exit_on_error; /* exit on parse and compile errors */
    CommentInput      comments;
    Value             format;      /* where to print this input formatted, or 0 to run it */
    Bool              format_full; /* whether in full, or as a summary */
} LexInput;

static LexInput *lexInput;
#define FILE_END_CHAR -2

/* Tokens read from all inputs */
static LexToken lexTokenCount;

static void
lexInputMark(void *object)
{
    LexInput *lex = object;

    MemReference(lex->next);
    MemReference(lex->file);
    MemReference(lex->comments.pending);
    MemReference(lex->comments.file_comments);
    MemReference(lex->format);
}

/* Place the comments waiting for this token, and count it */
static void
lexNoteToken(int token)
{
    if (!lexInput || token == NL || token == ENDFILE) {
        int line = lexInput ? lexInput->lineno : 0;

        /* No tokens, just before the next one */
        yylloc = (LexLocation) {
            .first_token = lexTokenCount + 1,
            .last_token = lexTokenCount,
            .first_line = line,
            .last_line = line,
        };
        return;
    }
    ++lexTokenCount;
    yylloc = (LexLocation) {
        .first_token = lexTokenCount,
        .last_token = lexTokenCount,
        .first_line = lexInput->lineno,
        .last_line = lexInput->lineno,
    };
    CommentNoteToken(lexTokenCount, token, lexInput->lineno);
    CommentInputToken(&lexInput->comments, lexTokenCount, lexInput->lineno);
}

/*
 * Comments after an input's last token join its file record, which is
 * kept by the input's name
 */
static void
lexFinishInput(void)
{
    ENTER();
    CommentPtr record = CommentInputFinish(&lexInput->comments);

    if (lexInput->format) {
        PrettyFormat(lexInput->format, record, lexInput->comments.first_line,
                     lexInput->format_full);
        NamespaceRestore();
    }
    CommentKeepRecord(lexInput->name, record);
    EXIT();
}

/*
 * Where to print the current input formatted, or 0 when it runs as
 * usual
 */
Value
LexFormatOutput(void)
{
    if (lexInput)
        return lexInput->format;
    return 0;
}

/*
 * Whether an input being formatted has started, so that what runs now,
 * such as a library it loads, comes from it
 */
Bool
LexFormatting(void)
{
    for (LexInput *lex = lexInput; lex; lex = lex->next)
        if (lex->format && lex->lineno > 0)
            return True;
    return False;
}

/*
 * Read the file in, named name, next, parsing it without running it,
 * and print it formatted to out, in full or as a summary
 */
void
LexFormatFile(Value in, Atom name, Value out, Bool full)
{
    NewLexInput(in, name, False, False);
    lexInput->format = out;
    lexInput->format_full = full;
}

/* Record a comment just read, which started on first_line and ends on this line */
static void
lexRecordComment(Value text, int first_line, CommentStyle style, Bool fall_through)
{
    CommentInputRecord(&lexInput->comments, text, first_line, lexInput->lineno, style, fall_through,
                       lexTokenCount);
}

/*
 * Drop the placed comments of tokens the parser has consumed, keeping
 * those of its lookahead token
 */
void
LexReleaseComments(Bool lookahead)
{
    CommentReleasePlaced(lookahead ? lexTokenCount : lexTokenCount + 1);
}

static DataType lexInputType = { lexInputMark, 0, "lexInputType" };

void
NewLexInput(Value file, Atom name, Bool after, Bool interactive)
{
    ENTER();
    LexInput *lex;

    lex = MemAllocate(&lexInputType, sizeof(*lex));
    lex->file = file;
    lex->name = name;
    lex->lineno = 0;
    lex->at_bol = True;
    lex->at_eof = False;
    lex->interactive = interactive;
    /* Files loaded by this input share its error handling */
    lex->exit_on_error = lexInput ? lexInput->exit_on_error : False;
    lex->comments = (CommentInput) {};
    lex->format = 0;
    lex->format_full = False;
    if (after) {
        LexInput **prev;

        for (prev = &lexInput; *prev; prev = &(*prev)->next)
            ;
        lex->next = 0;
        *prev = lex;
    } else {
        lex->next = lexInput;
        lexInput = lex;
    }
    EXIT();
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

static int LexGetChar(void);

static int
ReadlineGetChar(FILE *f)
{
    (void)f;
    return LexGetChar();
}
#endif

static ReferencePtr LexInputReference;

void
LexInit(void)
{
    ENTER();

#ifdef HAVE_LIBREADLINE
    rl_getc_function = ReadlineGetChar;
    rl_prep_term_function = (void *)my_prep_terminal;
    rl_deprep_term_function = my_deprep_terminal;
#ifdef HAVE_RL_CATCH_SIGNALS
    rl_catch_signals = 0;
#endif
#endif
    LexInputReference = NewReference((void **)&lexInput);
    MemAddRoot(LexInputReference);
    lexValueReference = NewReference((void **)&lexValue);
    MemAddRoot(lexValueReference);
    CommentInit();
    EXIT();
}

Atom
LexFileName(void)
{
    if (lexInput)
        return lexInput->name;
    return AtomId("<initialization>");
}

int
LexFileLine(void)
{
    if (lexInput)
        return lexInput->lineno;
    return 0;
}

Bool
LexInteractive(void)
{
    if (lexInput)
        return lexInput->interactive;
    return False;
}

/*
 * Set whether parse and compile errors in the current input, the most
 * recently added one, exit with an error status, as for files and
 * expressions given on the command line
 */
void
LexSetExitOnError(Bool exit)
{
    if (lexInput)
        lexInput->exit_on_error = exit;
}

void
LexErrorExit(void)
{
    if (lexInput && lexInput->exit_on_error) {
        IoFini();
        FileFini();
        exit(1);
    }
}

Bool
LexResetInteractive(void)
{
    while (lexInput->next && !lexInput->interactive) {
        FileClose(lexInput->file);
        lexInput = lexInput->next;
    }
    if (lexInput->interactive)
        return True;
    return False;
}

Bool
LexFile(char *s, Bool complain, Bool after)
{
    Value f;
    int   err;

    f = FileFopen(s, "r", &err);
    if (f == 0) {
        if (complain)
            (void)FilePrintf(FileStderr, "%s: %s\n", s,
                             StringChars(&FileGetErrorMessage(err)->string));
        return False;
    }
    (void)NewLexInput(f, AtomId(s), after, False);
    return True;
}

static int
LexGetChar(void)
{
    int c;

    for (;;) {
        c = FileInput(lexInput->file);
        if (c >= 0)
            return c;
        if (c == FileBlocked)
            ThreadsRun(0, lexInput->file);
        else {
            if (!lexInput->next)
                return FileEOF;
            lexInput->at_eof = True;
            return '\n';
        }
    }
}

static Value
prompt(void)
{
    Value v;

    if (parseIgnorenl || lexIgnorenl)
        v = lookupVar(0, "prompt2");
    else if (CurrentFrame)
        v = lookupVar(0, "prompt3");
    else
        v = lookupVar(0, "prompt");
    return v;
}

static int
LexGetInteractiveChar(void)
{
#ifdef HAVE_LIBREADLINE
    static char *line, *line_base;
    int          c;

    if (!line) {
        char *p;

        p = StrzPart(prompt(), "invalid prompt");
        if (!p)
            p = "??? ";
        for (;;) {
            FileFlush(FileStdout, False);
            FileFlush(FileStderr, False);
            if (FileStdout->file.flags & FileOutputBlocked)
                ThreadsRun(0, FileStdout);
            else if (FileStderr->file.flags & FileOutputBlocked)
                ThreadsRun(0, FileStderr);
            else
                break;
        }
        rl_inhibit_completion = 1;
        line_base = readline(p);
        line = line_base;
        if (!line)
            return FileEOF;
        add_history(line_base);
    }
    c = (*line++) & 0xff;
    if (!c) {
        c = '\n';
        free(line_base);
        line = 0;
    }
    return c;
#else
    if (lexInput->at_bol) {
        Value v = prompt();
        Print(FileStdout, v, 's', 0, 0, 0, ' ');
        FileFlush(FileStdout, True);
    }
    return LexGetChar();
#endif
}

#undef YY_INPUT
#define YY_NO_UNPUT

static int
yy_input(char *buf, int max_size)
{
    int c;
    int result = 0;

    while (result < max_size) {
        if (lexInput->at_eof) {
            lexFinishInput();
            FileClose(lexInput->file);
            lexInput = lexInput->next;
        }
        if (lexInput->interactive)
            c = LexGetInteractiveChar();
        else
            c = LexGetChar();
        if (lexInput->at_bol) {
            lexInput->lineno++;
        }
        lexInput->at_bol = False;
        if (c < 0)
            break;
        buf[result++] = (char)c;
        if (c == '\n') {
            lexInput->at_bol = True;
            break;
        }
    }
    return result;
}

#define YY_INPUT(buf, result, max_size) ((result) = yy_input(buf, max_size))

#ifndef FLEX_SCANNER
#undef input
#undef unput
int
input(void)
{
    char buf[1];
    int  r;

    YY_INPUT(buf, r, 1);
    if (r == 0)
        return 0;
    return buf[0];
}

void
unput(char c)
{
    if (c == '\n')
        lexInput->lineno--;
    FileUnput(lexInput->file, c);
}
#endif
%}

/* Generated by mkutf8names.py: names' UTF-8 characters */
/*
 * Names may hold any UTF-8 character but those operators use, so
 * that a name next to an operator lexes as it reads. Above each
 * definition splitting byte ranges around some of those characters
 * are the characters, their bytes, and the tokens of the rules using
 * them. Regenerate these with ninja utf8-names after changing those
 * operators
 */
U8CONT      [\x80-\xbf]
/*
 * ¬ \xc2\xac: LNOT
 * ² \xc2\xb2: POW2
 * ³ \xc2\xb3: POW3
 */
U8C2_A      \xc2([\x80-\xab\xad-\xb1\xb4-\xbf])
/*
 * × \xc3\x97: ASSIGNTIMES TIMES
 * ÷ \xc3\xb7: ASSIGNDIVIDE DIVIDE
 */
U8C3_A      \xc3([\x80-\x96\x98-\xb6\xb8-\xbf])
/*
 * … \xe2\x80\xa6: DOTDOTDOT
 */
U8E2_A      \xe2([\x81-\x85\x8a-\x96\x98-\xa6\xa8-\xa9\xac-\xbf]{U8CONT}|\x80[\x80-\xa5\xa7-\xbf])
/*
 * ↑ \xe2\x86\x91: ASSIGNPOW STARSTAR
 * → \xe2\x86\x92: ARROW
 * ⇒ \xe2\x87\x92: DARROW
 * ∧ \xe2\x88\xa7: AND
 * ∨ \xe2\x88\xa8: OR
 */
U8E2_B      \xe2(\x86[\x80-\x90\x93-\xbf]|\x87[\x80-\x91\x93-\xbf]|\x88[\x80-\xa6\xa9-\xbf])
/*
 * ≠ \xe2\x89\xa0: NE
 * ≤ \xe2\x89\xa4: LE
 * ≥ \xe2\x89\xa5: GE
 * ◊ \xe2\x97\x8a: VOIDVAL
 */
U8E2_C      \xe2(\x89[\x80-\x9f\xa1-\xa3\xa6-\xbf]|\x97[\x80-\x89\x8b-\xbf])
/*
 * ⧺ \xe2\xa7\xba: INC
 * ⪡ \xe2\xaa\xa1: ASSIGNSHIFTL SHIFTL
 * ⪢ \xe2\xaa\xa2: ASSIGNSHIFTR SHIFTR
 * ⫽ \xe2\xab\xbd: ASSIGNDIV DIV
 */
U8E2_D      \xe2(\xa7[\x80-\xb9\xbb-\xbf]|\xaa[\x80-\xa0\xa3-\xbf]|\xab[\x80-\xbc\xbe-\xbf])
U8LETTER_2  {U8C2_A}|{U8C3_A}|[\xc4-\xdf]{U8CONT}
U8LETTER_3  {U8E2_A}|{U8E2_B}|{U8E2_C}|{U8E2_D}|[\xe0-\xe1\xe3-\xef]{U8CONT}{U8CONT}
U8LETTER_4  [\xf0-\xf4]{U8CONT}{U8CONT}{U8CONT}
U8LETTER    {U8LETTER_2}|{U8LETTER_3}|{U8LETTER_4}
/* End of mkutf8names.py output */

%%
"/\052"         {
                    lexBlockComment();
                }
^[ \t]*#        lexHashComment();
"/\052"[ \t]*"[[fallthrough]]"[ \t]*"\052/" {
                    lexFallThroughComment(comment_block);
                }
^[ \t]*#[ \t]*"[[fallthrough]]"[ \t]*\n {
                    lexFallThroughComment(comment_hash);
                }
auto            { return yylvalClass(class_auto, AUTO); }
const           { return yylvalClass(class_const, CONST); }
global          { return yylvalClass(class_global, GLOBAL); }
static          { return yylvalClass(class_static, STATIC); }
function        { return yylvalType(typePrim[rep_void], FUNCTION); }
while           { return yylvalInts(WHILE); }
for             { return yylvalInts(FOR); }
do              { return yylvalInts(DO); }
if              { return yylvalInts(IF); }
else            { return yylvalInts(ELSE); }
switch          { return yylvalInts(SWITCH); }
break           { return yylvalInts(BREAK); }
continue        { return yylvalInts(CONTINUE); }
case            { return yylvalInts(CASE); }
default         { return yylvalInts(DEFAULT); }
return          { return yylvalInts(RETURNTOK); }
try             { return yylvalInts(TRY); }
catch           { return yylvalInts(CATCH); }
twixt           { return yylvalInts(TWIXT); }

poly            { return yylvalType(typePoly, POLY); }
bool            { return yylvalType(typePrim[rep_bool], BOOL); }
int             { return yylvalType(typePrim[rep_integer], INTEGER); }
rational        { return yylvalType(typePrim[rep_rational], RATIONAL); }
real            { return yylvalType(typePrim[rep_float], REAL); }
complex         { return yylvalType(typePrim[rep_complex], COMPLEX); }
string          { return yylvalType(typePrim[rep_string], STRING); }
file            { return yylvalType(typePrim[rep_file], FILET); }
semaphore       { return yylvalType(typePrim[rep_semaphore], SEMAPHORE); }
continuation    { return yylvalType(typePrim[rep_continuation], CONTINUATION); }
thread          { return yylvalType(typePrim[rep_thread], THREAD); }
struct          { return yylvalInts(STRUCT); }
union           { return yylvalInts(UNION); }
enum            { return yylvalInts(ENUM); }
void            { return yylvalType(typePrim[rep_void], VOID); }
noreturn        { return yylvalType(typeNoReturn, NORETURN); }
foreign         { return yylvalType(typePrim[rep_foreign], FOREIGN); }
true            { return yylvalValue(TrueVal, BOOLVAL); }
false           { return yylvalValue(FalseVal, BOOLVAL); }

typedef         { return yylvalInts(TYPEDEF); }
func            { return yylvalInts(FUNC); }
fork            { return yylvalInts(FORK); }
namespace       { return yylvalInts(NAMESPACE); }
import          { return yylvalInts(IMPORT); }
exception       { return yylvalInts(EXCEPTION); }
raise           { return yylvalInts(RAISE); }
protected       { return yylvalPublish(publish_protected, PROTECTED); }
public          { return yylvalPublish(publish_public, PUBLIC); }
extend          { return yylvalPublish(publish_extend, EXTEND); }
is_type         { return yylvalInts(ISTYPE); }
has_member      { return yylvalInts(HASMEMBER); }
";"             { return yylvalInts(SEMI); }
","             { return yylvalInts(COMMA); }
"$"             { return yylvalInts(DOLLAR); }
"..."           { return yylvalInts(DOTDOTDOT); }
"…"             { return yylvalUtf8(DOTDOTDOT); }
"."             { return yylvalInts(DOT); }
"->"            { return yylvalInts(ARROW); }
"→"             { return yylvalUtf8(ARROW); }
"=>"            { return yylvalInts(DARROW); }
"⇒"             { return yylvalUtf8(DARROW); }
"<>"            { return yylvalInts(VOIDVAL); }
"◊"             { return yylvalUtf8(VOIDVAL); }
\n              {
                    if (lexInput->at_eof) {
                        strcpy(yytext, "EOF");
                        return yylvalInts(ENDFILE);
                    }
                    if (!parseIgnorenl && !lexIgnorenl) {
                        return yylvalInts(NL);
                    }
                }
"("             { ++lexIgnorenl; return yylvalInts(OP); }
")"             { if (lexIgnorenl > 0) --lexIgnorenl; return yylvalInts(CP); }
"*["            { ++lexIgnorenl; return yylvalInts(STAROS); }
"["             { ++lexIgnorenl; return yylvalInts(OS); }
"]"             { if (lexIgnorenl > 0) --lexIgnorenl; return yylvalInts(CS); }
"{"             { ++lexIgnorenl; return yylvalInts(OC); }
"}"             { if (lexIgnorenl > 0) --lexIgnorenl; return yylvalInts(CC); }
"+="            { return yylvalInts(ASSIGNPLUS); }
"-="            { return yylvalInts(ASSIGNMINUS); }
"*="            { return yylvalInts(ASSIGNTIMES); }
"×="            { return yylvalUtf8(ASSIGNTIMES); }
"/="            { return yylvalInts(ASSIGNDIVIDE); }
"÷="            { return yylvalUtf8(ASSIGNDIVIDE); }
"//="           { return yylvalInts(ASSIGNDIV); }
"⫽="            { return yylvalUtf8(ASSIGNDIV); }
"%="            { return yylvalInts(ASSIGNMOD); }
"**="           { return yylvalInts(ASSIGNPOW); }
"↑="            { return yylvalUtf8(ASSIGNPOW); }
"<<="           { return yylvalInts(ASSIGNSHIFTL); }
"⪡="            { return yylvalUtf8(ASSIGNSHIFTL); }
">>="           { return yylvalInts(ASSIGNSHIFTR); }
"⪢="            { return yylvalUtf8(ASSIGNSHIFTR); }
"^="            { return yylvalInts(ASSIGNLXOR); }
"&&="           { return yylvalInts(ASSIGNAND); }
"||="           { return yylvalInts(ASSIGNOR); }
"&="            { return yylvalInts(ASSIGNLAND); }
"|="            { return yylvalInts(ASSIGNLOR); }
"="             { return yylvalInts(ASSIGN); }

"+"             { return yylvalInts(PLUS); }
"-"             { return yylvalInts(MINUS); }
"*"             { return yylvalInts(TIMES); }
"×"             { return yylvalUtf8(TIMES); }
"/"             { return yylvalInts(DIVIDE); }
"÷"             { return yylvalUtf8(DIVIDE); }
"//"            { return yylvalInts(DIV); }
"⫽"             { return yylvalUtf8(DIV); }
"**"            { return yylvalInts(STARSTAR); }
"↑"             { return yylvalUtf8(STARSTAR); }
"%"             { return yylvalInts(MOD); }
"!"             { return yylvalInts(BANG); }
"#"             { return yylvalInts(POUND); }
"&"             { return yylvalInts(LAND); }
"|"             { return yylvalInts(LOR); }
"^"             { return yylvalInts(LXOR); }
"~"             { return yylvalInts(LNOT); }
"¬"             { return yylvalUtf8(LNOT); }
"++"            { return yylvalInts(INC); }
"⧺"             { return yylvalUtf8(INC); }
"--"            { return yylvalInts(DEC); }
"=="            { return yylvalInts(EQ); }
"!="            { return yylvalInts(NE); }
"≠"             { return yylvalUtf8(NE); }
"<"             { return yylvalInts(LT); }
">"             { return yylvalInts(GT); }
"<="            { return yylvalInts(LE); }
"≤"             { return yylvalUtf8(LE); }
">="            { return yylvalInts(GE); }
"≥"             { return yylvalUtf8(GE); }
"&&"            { return yylvalInts(AND); }
"∧"             { return yylvalUtf8(AND); }
"||"            { return yylvalInts(OR); }
"∨"             { return yylvalUtf8(OR); }
"<<"            { return yylvalInts(SHIFTL); }
"⪡"             { return yylvalUtf8(SHIFTL); }
">>"            { return yylvalInts(SHIFTR); }
"⪢"             { return yylvalUtf8(SHIFTR); }
"?"             { return yylvalInts(QUEST); }
"::"            { return yylvalInts(COLONCOLON); }
":"             { return yylvalInts(COLON); }
"²"             { return yylvalUtf8(POW2); }
"³"             { return yylvalUtf8(POW3); }
" "             ;
"\t"            ;
"\r"            ;
\'([^\n\'\\]|\\.)*\'    {
                    char     *s;
                    ptrdiff_t len = yyleng - 1;
                    uint32_t  c;

                    s = yytext + 1;
                    s = StringNextChar(s, &c, &len);
                    if (c == '\\') {
                        if (!(s = StringNextChar(s, &c, &len)))
                            c = 0;
                        else if ('0' <= c && c <= '7') {
                            uint32_t ch;
                            char    *ps = s;

                            c = c - '0';
                            while ((s = StringNextChar(s, &ch, &len)) && '0' <= ch && ch <= '7') {
                                c = (c << 3) | (ch - '0');
                                ps = s;
                            }
                            s = ps;
                        } else if (c == 'x' && len > 0 && isxdigit((unsigned char)*s)) {
                            /* As in C, \x takes every hex digit that follows */
                            uint32_t ch;
                            char    *ps = s;

                            c = 0;
                            while ((s = StringNextChar(s, &ch, &len)) && ch < 0x80
                                   && isxdigit((int)ch)) {
                                c = (c << 4) | lexHexDigit(ch);
                                ps = s;
                            }
                            s = ps;
                        } else
                            c = lexEscape(c);
                    }
                    return yylvalValue(NewCharInteger(c), CHAR_CONST);
                }
\"([^\n\"]|\\\")*\"     {
                    ENTER();
                    char     *d, *s;
                    uint32_t  c;
                    ptrdiff_t len = yyleng - 2;
                    Value     value = NewString(len);

                    d = StringChars(&value->string);
                    s = yytext + 1;
                    while ((s = StringNextChar(s, &c, &len))) {
                        if (c == '\\') {
                            if (!(s = StringNextChar(s, &c, &len)))
                                break;
                            if ('0' <= c && c <= '7') {
                                uint32_t  ch;
                                char     *ps = s;
                                ptrdiff_t plen = len;

                                c = c - '0';
                                while ((s = StringNextChar(s, &ch, &len)) && '0' <= ch
                                       && ch <= '7') {
                                    c = (c << 3) | (ch - '0');
                                    ps = s;
                                    plen = len;
                                }
                                s = ps;
                                len = plen;
                            } else if (c == 'x' && len > 0 && isxdigit((unsigned char)*s)) {
                                /* As in C, \x takes every hex digit that follows */
                                uint32_t  ch;
                                char     *ps = s;
                                ptrdiff_t plen = len;

                                c = 0;
                                while ((s = StringNextChar(s, &ch, &len)) && ch < 0x80
                                       && isxdigit((int)ch)) {
                                    c = (c << 4) | lexHexDigit(ch);
                                    ps = s;
                                    plen = len;
                                }
                                s = ps;
                                len = plen;
                            } else
                                c = lexEscape(c);
                        }
                        d += StringPutChar(c, d);
                    }
                    *d = '\0';
                    value->string.length = d - StringChars(&value->string);
                    EXIT();
                    return yylvalValue(value, STRING_CONST);
                }
0                           { return yylvalNumber(TEN_NUM); }
0[0-7_]+                    { return yylvalNumber(OCTAL0_NUM); }
0o[0-7_]+                   { return yylvalNumber(OCTAL_NUM); }
0o[0-7_]+\./\.\.\.          { return yylvalNumber(OCTAL_FLOAT); }
0o[0-7_]+/\.\.              { return yylvalNumber(OCTAL_NUM); }
0o(([0-7_]+((\.[0-7_]*(\{[0-7_]+\})?)?))|(\.[0-7_]+)|(\.[0-7_]*\{[0-7_]+\}))(([Ee][-+]?[0-9_]+)?) {
                    return yylvalNumber(OCTAL_FLOAT);
                }
0b[01_]+                    { return yylvalNumber(BINARY_NUM); }
0b[01_]+\./\.\.\.           { return yylvalNumber(BINARY_FLOAT); }
0b[01_]+/\.\.               { return yylvalNumber(BINARY_NUM); }
0b(([01_]+((\.[01_]*(\{[01_]+\})?)?))|(\.[01_]+)|(\.[01_]*\{[01_]+\}))(([Ee][-+]?[0-9_]+)?) {
                    return yylvalNumber(BINARY_FLOAT);
                }
0x[0-9a-fA-F_]+             { return yylvalNumber(HEX_NUM); }
0x[0-9a-fA-F_]+\./\.\.\.    { return yylvalNumber(HEX_FLOAT); }
0x[0-9a-fA-F_]+/\.\.        { return yylvalNumber(HEX_NUM); }
0x(([0-9a-fA-F_]+((\.[0-9a-fA-F_]*(\{[0-9a-fA-F_]+\})?)?))|(\.[0-9a-fA-F_]+)|(\.[0-9a-fA-F_]*\{[0-9a-fA-F_]+\}))(([Pp][-+]?[0-9_]+)?) {
                    return yylvalNumber(HEX_FLOAT);
                }
[0-9][0-9_]*                { return yylvalNumber(TEN_NUM); }
[0-9][0-9_]*\./\.\.\.       { return yylvalNumber(TEN_FLOAT); }
[0-9][0-9_]*/\.\.           { return yylvalNumber(TEN_NUM); }
(([0-9][0-9_]*((\.[0-9_]*(\{[0-9_]+\})?)?))|(\.[0-9][0-9_]*)|(\.([0-9][0-9_]*)?\{[0-9_]+\}))(([Ee][-+]?[0-9_]+)?) {
                    return yylvalNumber(TEN_FLOAT);
                }
([a-zA-Z_]|{U8LETTER})([0-9a-zA-Z_]|{U8LETTER})* {
                    CommandPtr c;
                    SymbolPtr  symbol;
                    Atom       atom = AtomId(yytext);

                    if (!notCommand && (c = CommandFind(CurrentCommands, atom))) {
                        if (c->names)
                            return yylvalAtom(atom, NAMECOMMAND);
                        return yylvalAtom(atom, COMMAND);
                    }
                    if (LexNamespace)
                        symbol = NamespaceFindName(LexNamespace, atom, False);
                    else
                        symbol = NamespaceFindName(CurrentNamespace, atom, True);
                    if (symbol) {
                        switch (symbol->symbol.class) {
                        case class_namespace:
                            return yylvalAtom(atom, NAMESPACENAME);
                        case class_typedef:
                            return yylvalAtom(atom, TYPENAME);
                        default:
                            break;
                        }
                    }
                    return yylvalAtom(atom, NAME);
                }
.               FilePrintf (FileStderr, "character \\%o ignored\n", *yytext & 0xff);
%%

uint32_t
lexEscape(uint32_t c)
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

/*
 * Read the rest of a comment whose opening slash-star has been read,
 * returning it as written, or 0 when it never ends
 */
static Value
lexReadComment(void)
{
    ENTER();
    int   c, prev = 0;
    Value f = FileStringWrite();

    FilePuts(f, "/*");
    for (;;) {
        c = input();
        if (c == EOF || lexInput->at_eof) {
            yyerror("Missing */ at end of file");
            RETURN(0);
        }
        FileOutput(f, c);
        if (prev == '*' && c == '/')
            break;
        prev = c;
    }
    RETURN(FileStringString(f));
}

/* Read and record a comment whose opening slash-star has been read */
static void
lexBlockComment(void)
{
    ENTER();
    int   first_line = lexInput->lineno;
    Value text = lexReadComment();

    if (text)
        lexRecordComment(text, first_line, comment_block, False);
    EXIT();
}

/*
 * Record the comment in yytext, '[[fallthrough]]' alone, which marks a
 * switch case falling through to the next as C's attribute does; a
 * '#' comment's text ends before its newline
 */
static void
lexFallThroughComment(CommentStyle style)
{
    ENTER();
    ptrdiff_t len = (ptrdiff_t)strlen(yytext);
    Value     text;

    if (style == comment_hash && len && yytext[len - 1] == '\n')
        len--;
    text = NewString(len);
    memcpy(StringChars(&text->string), yytext, (size_t)len);
    StringChars(&text->string)[len] = '\0';
    lexRecordComment(text, lexInput->lineno, style, True);
    EXIT();
}

/*
 * Read and record the rest of a line starting with '#', whose start
 * through the '#' is in yytext
 */
static void
lexHashComment(void)
{
    ENTER();
    int          first_line = lexInput->lineno;
    CommentStyle style = comment_hash;
    int          c;
    Value        f = FileStringWrite();
    Bool         at_start = yytext[0] == '#';

    /* input() may overwrite yytext */
    FilePuts(f, yytext);
    c = input();
    if (first_line == 1 && at_start && c == '!')
        style = comment_shebang;
    while (c != EOF && c != '\n') {
        FileOutput(f, c);
        c = input();
    }
    lexRecordComment(FileStringString(f), first_line, style, False);
    EXIT();
}

Value
atov(char *s, int base)
{
    ENTER();
    Value result;
    Value b;
    char  c;
    int   i;

    b = NewInt(base);
    result = NewInt(0);
    for (;;) {
        c = *s++;
        if ('0' <= c && c <= '9')
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
        result = Plus(NewInt(i), Times(result, b));
    }
    RETURN(result);
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

/* Count the digits that atov will convert */
static int
number_len(char *s, int base)
{
    int  len = 0;
    char c;
    int  i;

    for (;;) {
        c = *s++;
        if ('0' <= c && c <= '9')
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
        len++;
    }
    return len;
}

Value
aetov(char *s)
{
    ENTER();
    char *int_part, *frac_part, *rep_part, *exp_part, *next;
    int   sign, int_len = -1, frac_len = -1, rep_len = -1, exp_frac_len = -1, esign;
    Value v, sv;
    char  exp_char;
    int   base = 10;
    int   exp_base;

    if (s[0] == '0') {
        switch (s[1]) {
        case 'x':
        case 'X':
            base = 16;
            s += 2;
            break;
        case 'o':
        case 'O':
            base = 8;
            s += 2;
            break;
        case 'b':
        case 'B':
            base = 2;
            s += 2;
            break;
        }
    }

    switch (base) {
    case 2:
        exp_char = 'e';
        exp_base = 2;
        break;
    case 8:
        exp_char = 'e';
        exp_base = 2;
        break;
    case 10:
    default:
        exp_char = 'e';
        exp_base = 10;
        break;
    case 16:
        exp_char = 'p';
        exp_base = 2;
        break;
    }
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
        if (rep_part) {
            rep_part++;
            next = rep_part;
            next = scan_number(next, '}', &rep_len);
            /*
             * Constants from the lexer close their repeat groups, but
             * string_to_real's needn't; like other stray characters, an
             * unclosed group ends the number
             */
            if (!next) {
                next = rep_part + strlen(rep_part);
                rep_part = 0;
            }
        }
    }
    exp_part = scan_number(next, exp_char, &exp_frac_len);
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
        frac_len = number_len(frac_part, base);
    v = atov(int_part, base);
    if (frac_part) {
        v = Plus(v, Divide(atov(frac_part, base), Pow(NewInt(base), NewInt(frac_len))));
    }
    if (rep_part) {
        Value rep;

        rep = Divide(atov(rep_part, base), Minus(Pow(NewInt(base), NewInt(rep_len)), One));
        if (frac_len)
            rep = Divide(rep, Pow(NewInt(base), NewInt(frac_len)));
        v = Plus(v, rep);
    }
    if (exp_part) {
        sv = Pow(NewInt(exp_base), atov(exp_part, 10));
        if (esign > 0)
            v = Times(v, sv);
        else
            v = Divide(v, sv);
    }
    if (sign == -1)
        v = Negate(v);
    RETURN(v);
}
