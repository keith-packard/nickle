/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * pretty.c
 *
 * pretty print a function
 */

#include <stdio.h>
#include <string.h>
#include "nickle.h"
#include "gram.h"
#include "ref.h"

#define PRETTY_NUM_WIDTH 10

/*
 * Printed lines end by column PRETTY_WIDTH, and lines of code, other
 * than comments, also span at most PRETTY_CODE_WIDTH columns from their
 * first non-blank character
 */
#define PRETTY_WIDTH      100
#define PRETTY_CODE_WIDTH 80

/*
 * While marking, the expression printer brackets the parts of an
 * expression where a line may break, for PrettyWrapExpr: a group
 * starts with PRETTY_GROUP and a kind, ends with PRETTY_END, and
 * PRETTY_BREAK separates its items. PRETTY_NEWLINE is a line break a
 * comment ending its line forces, in the group holding it. Printed code
 * escapes these bytes in strings and characters, so they appear nowhere
 * else
 */
#define PRETTY_GROUP      '\001'
#define PRETTY_END        '\002'
#define PRETTY_BREAK      '\003'
#define PRETTY_NEWLINE    '\004'

#define PRETTY_CALL       'c' /* arguments, aligned after the '(' */
#define PRETTY_OPERATORS  'A' /* operands of one precedence, broken before operators */
#define PRETTY_CONDITION  't' /* ?: broken before ':', under the '?' */
#define PRETTY_INITIALIZE 'i' /* initializers, aligned after the '{ ' */
#define PRETTY_SHIFTED    'l' /* a lambda's block or struct's members, shifted to its column */
#define PRETTY_CLAUSES    's' /* a for's or twixt's clauses, each on a line, after the '(' */
#define PRETTY_COMMAS     'm' /* a comma expression's operands, each on a line */
#define PRETTY_SUBSCRIPT  'x' /* subscripts, aligned after the '[' */
#define PRETTY_MEMBERS    'd' /* member accesses, broken before each '.' or '->' */
#define PRETTY_CAST       'k' /* a cast and its value, broken after the cast */
#define PRETTY_ASSIGN     'a' /* a statement's assignment, its value moving past the '=' */
#define PRETTY_ROWS       'r' /* an initializer's rows, each starting a line */
#define PRETTY_ROW        'w' /* a row's items, aligned at the row's start */
#define PRETTY_NOTE       'n' /* a comment, kept whole */

/*
 * An operator chain's kind is PRETTY_OPERATORS plus the operators'
 * precedence, which ranks where it breaks
 */
#define PrettyChainKind(k) ((k) >= PRETTY_OPERATORS && (k) <= 'Z')

/*
 * How a group ranks as a place to break, nicest first: lists, then
 * operators from the loosest binding, ?: first. A group inside more
 * brackets ranks after any outside them, and breaking inside a last
 * resort, a subscript, member access, or cast, ranks after breaking
 * anywhere outside one
 */
#define PRETTY_RANKS 32
#define PRETTY_DEPTH 64 /* the depths a priority tells apart */

static int PrettyPrecedence(int token);

static ptrdiff_t
PrettyGroupRank(char kind)
{
    if (PrettyChainKind(kind))
        return kind - PRETTY_OPERATORS;
    if (kind == PRETTY_CONDITION)
        return PrettyPrecedence(QUEST);
    return 0;
}

/* Whether breaking a group of kind is a last resort */
static Bool
PrettyLastResort(char kind)
{
    return kind == PRETTY_SUBSCRIPT || kind == PRETTY_MEMBERS || kind == PRETTY_CAST;
}

/*
 * Whether breaking anything inside a group of kind is a last resort too,
 * as inside a subscript's brackets, but not inside a cast's value or the
 * operands of member accesses
 */
static Bool
PrettyLastResortInside(char kind)
{
    return kind == PRETTY_SUBSCRIPT;
}

/*
 * The priority of a group of kind, depth brackets in and inside inside
 * subscripts, counting itself; lower breaks first
 */
static ptrdiff_t
PrettyGroupPriority(char kind, ptrdiff_t depth, ptrdiff_t inside)
{
    ptrdiff_t resorts = inside;

    if (PrettyLastResort(kind) && !PrettyLastResortInside(kind))
        resorts++;
    if (depth >= PRETTY_DEPTH)
        depth = PRETTY_DEPTH - 1;
    return (resorts * PRETTY_DEPTH + depth) * PRETTY_RANKS + PrettyGroupRank(kind);
}

static Bool      prettyMarking;

/*
 * Columns right of column 0 the code being printed will go, when a
 * lambda's body or a struct type's members are printed as from column
 * 0 to be shifted to where it lands; lines end that much sooner
 */
static ptrdiff_t prettyShift;
#define PRETTY_LINE_END (PRETTY_WIDTH - prettyShift)

/*
 * The lambdas with blocks and initializers' struct types spanning lines
 * in the marked text being laid out, which print once laying it out has
 * placed them
 */
static Expr    **prettyShifted;
static ptrdiff_t prettyShiftedCount, prettyShiftedSize;
static Expr     *prettyChain;   /* the left operand continuing an operator chain */
static Expr     *prettyMembers; /* the member access continuing a chain of them */
static Expr     *prettyAssign;  /* the assignment whose value may move past its operator */
static Expr     *prettyHeld;    /* the initializer item whose rows print its comments before it */

static void
PrettyMark(Value f, char mark, char kind)
{
    if (prettyMarking) {
        FileOutput(f, mark);
        if (kind)
            FileOutput(f, kind);
    }
}

/* Whether code starting at column indent and ending at column end fits */
static Bool
PrettyCodeFits(ptrdiff_t indent, ptrdiff_t end)
{
    return end - indent <= PRETTY_CODE_WIDTH && end <= PRETTY_LINE_END;
}

static int
PrettyNumWidth(double_digit i)
{
    int w = 1;
    while (i >= 10) {
        w++;
        i /= 10;
    }
    return w;
}

static void
PrettyProfNum(Value f, double_digit i, int pad_left)
{
    int spaces = PRETTY_NUM_WIDTH - PrettyNumWidth(i);
    if (pad_left)
        while (spaces-- > 0)
            FilePuts(f, " ");
    FilePrintf(f, "%D", i);
    if (!pad_left)
        while (spaces-- > 0)
            FilePuts(f, " ");
}

static void  PrettyParameters(Value f, Expr *e, int level, Bool nest);
static void  PrettyArrayInit(Value f, Expr *e, int level, Bool nest);
static void  PrettyStatement(Value f, Expr *e, int level, int blevel, Bool nest);
static void  PrettyBody(Value f, CodePtr code, int level, Bool nest, Bool attach, ptrdiff_t col);
static void  PrettyWrapArgs(Value f, ArgType *args, int level, ptrdiff_t col, ptrdiff_t tail);
static Expr *PrettyShortBody(CodePtr code, Atom name, Value type, int level);
static Bool  PrettyShortDefinition(Value type, Value rest, CodePtr code, Atom name, Class class,
                                   Publish publish, int level);
static void  PrettyArgs(Value f, ArgType *args, int level, Bool comments);
static void  PrettyTypeLine(Value f, Type *t);
static void  PrettyComment(Value f, Value text, int level);
static ptrdiff_t PrettyColumns(const char *text, ptrdiff_t len);
static Bool      PrettyCommentOneLine(Value f, Value text, ptrdiff_t width);

/*
 * A line of code printed with others whose names and trailing comments
 * line up: its comment entries, leading ones then trailing ones; for a
 * declaration or member, its printed type and the rest of its line, the
 * names and what follows them; otherwise its printed text, or 0 for
 * comments alone; and the statement it prints, for profiling
 */
typedef struct _prettyMember {
    Expr *comments;
    Value type;
    Value rest;
    Value text;
    Expr *stat;
    Expr *decl; /* a declaration to print as the rest, once its column is known */
    Bool  nest;
    Bool  blank;    /* whether a blank line goes before it and its comments */
    Bool  spaced;   /* whether leading comments get a blank line above, as in a block */
    Bool  one_line; /* a definition printing as 'type name(args) = value;' */
} PrettyMember;

static void PrettyMembers(Value f, PrettyMember *m, ptrdiff_t n, int level, Bool newline);
static void PrettyDeclType(Value f, Expr *e, int level, Bool nest);
static void PrettyDeclNames(Value f, Expr *e, int level, Bool nest);
static void PrettyWrapDeclNames(Value f, Expr *e, int level, Bool nest, ptrdiff_t col, Bool paren);
static ptrdiff_t PrettyDeclNamesWidth(Expr *e, int level, Bool nest);
static ptrdiff_t PrettyFlatWidth(const char *s, ptrdiff_t len);
static ptrdiff_t PrettyCodeEnd(ptrdiff_t indent);
static void      PrettyTextFit(Value text, ptrdiff_t col, ptrdiff_t tail, ptrdiff_t *lines,
                               ptrdiff_t *over);
static void      PrettyWrapExpr(Value f, Expr *e, int prec, int level, Bool nest, ptrdiff_t col,
                                ptrdiff_t extra, ptrdiff_t tail, Bool paren);
static Bool      PrettyMultiline(Value text);
static Bool      PrettyHasLeading(Expr *c);
static void PrettyType(Value f, Type *t, int level, Bool comments, ptrdiff_t col, ptrdiff_t after);
static ptrdiff_t PrettyTextColumns(Value text);

static void
PrettyIndent(Value f, Expr *e, int level)
{
    int i;
    if (profiling) {
        if (e) {
            PrettyProfNum(f, e->base.sub_ticks + e->base.ticks, 1);
            FilePuts(f, " ");
            PrettyProfNum(f, e->base.ticks, 1);
        } else
            FilePuts(f, "                     ");
        FilePuts(f, ":  ");
    }
    for (i = 0; i < level; i++)
        FilePuts(f, "    ");
}

/* Columns taken by the last line of text, with tabs every eight */
static ptrdiff_t
PrettyLastLineColumns(const char *s, ptrdiff_t len)
{
    ptrdiff_t col = 0, i;

    for (i = 0; i < len; i++) {
        if (s[i] == '\n')
            col = 0;
        else if (s[i] == '\t')
            col = (col / 8 + 1) * 8;
        else if ((s[i] & 0xc0) != 0x80)
            col++;
    }
    return col;
}

/*
 * Print text, a printed statement or declaration, followed by the
 * trailing comments starting the list rest, returning what follows
 * them. A comment goes at the end of the text's last line when it fits
 * there on one line, starting at column when that is past the text and
 * they all fit there, and on its own lines before the text otherwise
 */
static Expr *
PrettyTrailingComments(Value f, Value text, Expr *rest, int level, Bool newline, ptrdiff_t column)
{
    ENTER();
    Value       after = FileStringWrite();
    Value       tail;
    const char *s;
    ptrdiff_t   len, col;

    s = StringChars(&text->string);
    len = text->string.length;
    if (len && s[len - 1] == '\n')
        len--;
    col = PrettyLastLineColumns(s, len);
    /* Comments that don't all fit on the line past column follow the text */
    if (column > col + 1) {
        ptrdiff_t at = column - 1;
        Expr     *c;

        for (c = rest; c && c->tree.left && c->tree.left->base.tag == COMMENT_CONST
             && c->tree.left->comment.comment->place == comment_trailing;
             c = c->tree.right) {
            Value one = FileStringWrite();

            if (!PrettyCommentOneLine(one, c->tree.left->comment.comment->text,
                                      PRETTY_LINE_END - at - 1)) {
                column = 0;
                break;
            }
            one = FileStringString(one);
            at += 1 + StringLength(StringChars(&one->string), one->string.length);
        }
    }
    for (; rest && rest->tree.left && rest->tree.left->base.tag == COMMENT_CONST
         && rest->tree.left->comment.comment->place == comment_trailing;
         rest = rest->tree.right) {
        Value comment = rest->tree.left->comment.comment->text;
        Value one = FileStringWrite();

        if (column > col + 1 && PrettyCommentOneLine(one, comment, PRETTY_LINE_END - column)) {
            tail = FileStringString(one);
            while (col < column - 1) {
                FileOutput(after, ' ');
                col++;
            }
            FileOutput(after, ' ');
            FilePutsc(after, StringChars(&tail->string), tail->string.length);
            col += 1 + StringLength(StringChars(&tail->string), tail->string.length);
        } else if (PrettyCommentOneLine(one, comment, PRETTY_LINE_END - col - 1)) {
            tail = FileStringString(one);
            FileOutput(after, ' ');
            FilePutsc(after, StringChars(&tail->string), tail->string.length);
            col += 1 + StringLength(StringChars(&tail->string), tail->string.length);
        } else {
            PrettyComment(f, comment, level);
            FileOutput(f, '\n');
        }
    }
    tail = FileStringString(after);
    FilePutsc(f, s, len);
    FilePutsc(f, StringChars(&tail->string), tail->string.length);
    if (newline)
        FileOutput(f, '\n');
    RETURN(rest);
}

/* Whether an entry of a comment list is a blank line from the source */
static Bool
PrettyBlankEntry(Expr *c)
{
    return c && c->tree.left && c->tree.left->base.tag == COMMENT_CONST
        && c->tree.left->comment.comment->style == comment_blank;
}

/*
 * Whether comment entries are all fall-through marks, '[[fallthrough]]',
 * apart from blank lines
 */
static Bool
PrettyFallThrough(Expr *c)
{
    Bool any = False;

    for (; c && c->tree.left; c = c->tree.right) {
        if (PrettyBlankEntry(c))
            continue;
        if (!c->tree.left->comment.comment->fall_through)
            return False;
        any = True;
    }
    return any;
}

/* Whether a statement defines a function, giving it a body */
static Bool
PrettyFunctionDefinition(Expr *statement)
{
    return statement->base.tag == FUNC && statement->decl.decl && statement->decl.decl->init;
}

/* Whether a statement is a declaration whose names can line up */
static Bool
PrettyAlignedDecl(Expr *statement)
{
    Expr *e;

    if (statement->base.tag != EXPR)
        return False;
    e = statement->tree.left;
    return e->base.tag == VAR && e->decl.class != class_exception && e->decl.decl;
}

/* Whether comment entries hold a header's doc comments */
static Bool
PrettyHasDoc(Expr *c)
{
    for (; c && c->tree.left; c = c->tree.right)
        if (c->tree.left->base.tag == COMMENT_CONST
            && c->tree.left->comment.comment->place == comment_doc)
            return True;
    return False;
}

static Bool PrettyHashText(Value text);

/*
 * Print a header's doc comments from a list, each on lines of its own
 * under it, one level in, returning whether the last was a '#' comment,
 * which ends its line
 */
static Bool
PrettyDocComments(Value f, Expr *c, int level)
{
    Bool hash = False;

    for (; c && c->tree.left; c = c->tree.right)
        if (c->tree.left->base.tag == COMMENT_CONST
            && c->tree.left->comment.comment->place == comment_doc) {
            FilePuts(f, "\n");
            PrettyComment(f, c->tree.left->comment.comment->text, level + 1);
            hash = PrettyHashText(c->tree.left->comment.comment->text);
        }
    return hash;
}

/* End a declaration's doc comments with its ';', under a '#' comment on a line of its own */
static void
PrettyDocSemicolon(Value f, Expr *c, int level)
{
    if (PrettyDocComments(f, c, level)) {
        FilePuts(f, "\n");
        PrettyIndent(f, 0, level + 1);
    }
    FilePuts(f, ";");
}

/* Clear the marks of comment entries */
static void
PrettyUnmarkList(Expr *c)
{
    for (; c; c = c->tree.right)
        c->tree.left->comment.comment->printed = False;
}

/* Clear the marks of the comments an expression holds, and its declared names */
static Bool
PrettyUnmarkComments(Expr *e, void *closure)
{
    DeclListPtr decl;

    (void)closure;
    PrettyUnmarkList(e->base.comments);
    if (e->base.tag == VAR)
        for (decl = e->decl.decl; decl; decl = decl->next)
            PrettyUnmarkList(decl->comments);
    return False;
}

/*
 * Collect the comments an expression or its declared names hold that printing it didn't
 * place, marking them placed: a statement holding this one won't
 * collect them again
 */
static void
PrettyUnplacedList(Expr *c, Expr ***last)
{
    for (; c; c = c->tree.right)
        if (!c->tree.left->comment.comment->printed) {
            c->tree.left->comment.comment->printed = True;
            **last = NewExprTree(OC, c->tree.left, 0);
            *last = &(**last)->tree.right;
        }
}

static Bool
PrettyUnplacedComments(Expr *e, void *closure)
{
    DeclListPtr decl;

    PrettyUnplacedList(e->base.comments, closure);
    if (e->base.tag == VAR)
        for (decl = e->decl.decl; decl; decl = decl->next)
            PrettyUnplacedList(decl->comments, closure);
    return False;
}

/*
 * Put the comments a statement's expressions hold that printing it
 * didn't place after its leading comments, before it
 */
static Expr *
PrettyHoistComments(Expr *statement, Expr *comments)
{
    ENTER();
    Expr *hoisted = 0, **last = &hoisted, **at;

    ExprVisitPreOrder(statement, PrettyUnplacedComments, &last);
    if (hoisted) {
        for (at = &comments; *at && (*at)->tree.left
             && (*at)->tree.left->comment.comment->place != comment_trailing;
             at = &(*at)->tree.right)
            ;
        *last = *at;
        *at = hoisted;
    }
    RETURN(comments);
}

/*
 * Print a statement list. Comments in it print only in nested output,
 * as for print and edit, not in the single statements the debugger
 * and error messages show; nested output also lines up declarations
 * and trailing comments as PrettyMembers does. A block whose '{' ends a
 * line of code, as 'if (a) {' does, is cuddled: a leading comment,
 * function definition, or namespace starting it has a blank line
 * above, which one after a '{' on a line of its own doesn't need
 */
static void
PrettyBlock(Value f, Expr *e, int level, Bool nest, Bool cuddled)
{
    ENTER();
    PrettyMember *m;
    ptrdiff_t     n, i;
    Expr         *s, *comments = 0, **last = &comments;

    if (!nest) {
        for (; e->tree.left; e = e->tree.right)
            PrettyStatement(f, e->tree.left, level, level, nest);
        EXIT();
        return;
    }
    for (n = 1, s = e; s->tree.left; s = s->tree.right)
        n++;
    m = AllocateTemp((size_t)n * sizeof(PrettyMember));
    n = 0;
    for (s = e;; s = s->tree.right) {
        Expr *statement = s->tree.left;

        if (statement && statement->base.tag == COMMENT_CONST) {
            /* The header prints the doc comments starting a body */
            if (statement->comment.comment->place == comment_doc)
                continue;
            *last = NewExprTree(OC, statement, 0);
            last = &(*last)->tree.right;
            continue;
        }
        if (!statement) {
            /* Comments after the last statement */
            if (comments) {
                *last = NewExprTree(OC, 0, 0);
                m[n].comments = comments;
                m[n].type = m[n].rest = m[n].text = 0;
                m[n].stat = m[n].decl = 0;
                m[n].nest = nest;
                m[n].one_line = False;
                n++;
            }
            break;
        }
        /* The statement's trailing comments follow it */
        while (s->tree.right->tree.left && s->tree.right->tree.left->base.tag == COMMENT_CONST
               && s->tree.right->tree.left->comment.comment->place == comment_trailing) {
            s = s->tree.right;
            *last = NewExprTree(OC, s->tree.left, 0);
            last = &(*last)->tree.right;
        }
        *last = NewExprTree(OC, 0, 0);
        m[n].comments = comments;
        m[n].stat = statement;
        m[n].type = m[n].rest = m[n].text = 0;
        m[n].decl = 0;
        m[n].nest = nest;
        m[n].one_line = False;
        /* Printing the statement marks the comments it places */
        ExprVisitPreOrder(statement, PrettyUnmarkComments, 0);
        if (PrettyAlignedDecl(statement)) {
            Value type = FileStringWrite();

            PrettyDeclType(type, statement->tree.left, level, nest);
            m[n].type = FileStringString(type);
            m[n].decl = statement->tree.left;
            /* PrettyMembers prints the names later, placing the same comments */
            PrettyDeclNames(FileStringWrite(), statement->tree.left, level, nest);
        } else if (statement->base.tag == IMPORT) {
            /* An import lines up with declarations, 'import' as its type */
            Value type = FileStringWrite();
            Value rest = FileStringWrite();

            FilePrintf(type, "%pimport", statement->tree.right->decl.publish);
            PrettyExpr(rest, statement->tree.left, -1, level, nest);
            FilePuts(rest, ";");
            m[n].type = FileStringString(type);
            m[n].rest = FileStringString(rest);
        } else {
            Value text = FileStringWrite();
            Value rest = FileStringWrite();

            /* A one-line definition lines up with declarations, its type as theirs */
            if (PrettyFunctionDefinition(statement)
                && PrettyShortDefinition(text, rest, statement->decl.decl->init->code.code,
                                         statement->decl.decl->name, statement->decl.class,
                                         statement->decl.publish, level)) {
                m[n].type = FileStringString(text);
                m[n].rest = FileStringString(rest);
                m[n].one_line = True;
            } else {
                PrettyStatement(text, statement, level, level, nest);
                m[n].text = FileStringString(text);
            }
        }
        m[n].comments = PrettyHoistComments(statement, m[n].comments);
        n++;
        comments = 0;
        last = &comments;
    }
    /*
     * A function definition or namespace has a blank line above its
     * comments and below it, unless the definition prints on one line,
     * as a declaration does
     */
    for (i = 0; i < n; i++) {
        m[i].blank = False;
        m[i].spaced = i > 0 || cuddled;
    }
    /*
     * So do the declarations and imports starting a block, unless one
     * statement or none follows them
     */
    for (i = 0; i < n && m[i].type; i++)
        ;
    if (i > 0) {
        ptrdiff_t r = i, following = 0;

        for (; i < n; i++)
            if (m[i].stat)
                following++;
        if (following > 1)
            m[r].blank = True;
    }
    /*
     * A leading comment has a blank line above it, as does a run of
     * them, except fall-through marks, which end the code above them
     */
    for (i = cuddled ? 0 : 1; i < n; i++)
        if (PrettyHasLeading(m[i].comments) && !PrettyFallThrough(m[i].comments))
            m[i].blank = True;
    for (i = 0; i < n; i++)
        if (m[i].stat && !m[i].one_line
            && (PrettyFunctionDefinition(m[i].stat) || m[i].stat->base.tag == NAMESPACE)) {
            if (i > 0 || cuddled)
                m[i].blank = True;
            if (i + 1 < n)
                m[i + 1].blank = True;
        }
    PrettyMembers(f, m, n, level, True);
    EXIT();
}

/* Precedence levels, in the order of gram.y's %left and %right lines */
static int
PrettyPrecedence(int token)
{
    switch (token) {
    /* A declaration is an expr, as low as a comma expression */
    case VAR:
    case COMMA:
        return 0;
    case ASSIGN:
    case ASSIGNPLUS:
    case ASSIGNMINUS:
    case ASSIGNTIMES:
    case ASSIGNDIVIDE:
    case ASSIGNMOD:
    case ASSIGNDIV:
    case ASSIGNPOW:
    case ASSIGNSHIFTL:
    case ASSIGNSHIFTR:
    case ASSIGNLXOR:
    case ASSIGNLAND:
    case ASSIGNLOR:
    case ASSIGNAND:
    case ASSIGNOR:
        return 1;
    case FORK:
        return 2;
    case QUEST:
    case COLON:
        return 3;
    case OR:
        return 4;
    case AND:
        return 5;
    case LOR:
        return 6;
    case LXOR:
        return 7;
    case LAND:
        return 8;
    case EQ:
    case NE:
        return 9;
    case LT:
    case GT:
    case LE:
    case GE:
        return 10;
    case SHIFTL:
    case SHIFTR:
        return 11;
    case PLUS:
    case MINUS:
        return 12;
    case TIMES:
    case DIVIDE:
    case MOD:
    case DIV:
        return 13;
    case POW:
        return 14;
    /* The grammar puts every prefix and postfix unary operator on one level */
    case UMINUS:
    case BANG:
    case FACT:
    case LNOT:
    case INC:
    case DEC:
    case STAR:
    case AMPER:
        return 15;
    case OS:
    case CS:
    case OP:
    case DOT:
    case ARROW:
        return 18;
    default:
        return 19;
    }
}

/*
 * Whether operands of the same precedence group to the right, so that
 * a op b op c means a op (b op c)
 */
static Bool
PrettyRightAssoc(int token)
{
    switch (token) {
    case ASSIGN:
    case ASSIGNPLUS:
    case ASSIGNMINUS:
    case ASSIGNTIMES:
    case ASSIGNDIVIDE:
    case ASSIGNMOD:
    case ASSIGNDIV:
    case ASSIGNPOW:
    case ASSIGNSHIFTL:
    case ASSIGNSHIFTR:
    case ASSIGNLXOR:
    case ASSIGNLAND:
    case ASSIGNLOR:
    case ASSIGNAND:
    case ASSIGNOR:
    case QUEST:
    case COLON:
    case POW:
        return True;
    default:
        return False;
    }
}

/* The kinds of binary operator PrettyClarifies tells apart */
typedef enum {
    pretty_op_other,
    pretty_op_arithmetic,
    pretty_op_shift,
    pretty_op_comparison,
    pretty_op_bitwise,
    pretty_op_and,
    pretty_op_or,
} PrettyOpKind;

static PrettyOpKind
PrettyOperatorKind(int token)
{
    switch (token) {
    case PLUS:
    case MINUS:
    case TIMES:
    case DIVIDE:
    case DIV:
    case MOD:
    case POW:
        return pretty_op_arithmetic;
    case SHIFTL:
    case SHIFTR:
        return pretty_op_shift;
    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
        return pretty_op_comparison;
    case LAND:
    case LOR:
    case LXOR:
        return pretty_op_bitwise;
    case AND:
        return pretty_op_and;
    case OR:
        return pretty_op_or;
    default:
        return pretty_op_other;
    }
}

/*
 * Whether an operand of op that is an operand operation prints in
 * parentheses, though precedence doesn't need them, as people misread
 * how such operators group (gcc's -Wparentheses): arithmetic, shifts,
 * comparisons, or another bitwise operator inside a bitwise operator,
 * arithmetic inside a shift, a comparison inside a comparison, and &&
 * inside ||
 */
static Bool
PrettyClarifies(int op, int operand)
{
    PrettyOpKind inner = PrettyOperatorKind(operand);

    switch (PrettyOperatorKind(op)) {
    case pretty_op_bitwise:
        return inner == pretty_op_arithmetic || inner == pretty_op_shift
            || inner == pretty_op_comparison || (inner == pretty_op_bitwise && operand != op);
    case pretty_op_shift:
        return inner == pretty_op_arithmetic;
    case pretty_op_comparison:
        return inner == pretty_op_comparison;
    case pretty_op_or:
        return inner == pretty_op_and;
    default:
        return False;
    }
}

/* The alternate Unicode spelling of the operator of e, when the source used it, else 0 */
static const char *
PrettyUtf8Operator(Expr *e)
{
    if (!(e->base.flags & EXPR_UTF8))
        return 0;
    switch (e->base.tag) {
    case TIMES:
        return "×";
    case DIVIDE:
        return "÷";
    case DIV:
        return "⫽";
    case POW:
        return "↑";
    case SHIFTL:
        return "⪡";
    case SHIFTR:
        return "⪢";
    case NE:
        return "≠";
    case LE:
        return "≤";
    case GE:
        return "≥";
    case AND:
        return "∧";
    case OR:
        return "∨";
    case ASSIGNTIMES:
        return "×=";
    case ASSIGNDIVIDE:
        return "÷=";
    case ASSIGNDIV:
        return "⫽=";
    case ASSIGNPOW:
        return "↑=";
    case ASSIGNSHIFTL:
        return "⪡=";
    case ASSIGNSHIFTR:
        return "⪢=";
    case LNOT:
        return "¬";
    case INC:
        return "⧺";
    case ARROW:
        return "→";
    case DARROW:
        return "⇒";
    case DOTDOTDOT:
        return "…";
    case VOIDVAL:
        return "◊";
    default:
        return 0;
    }
}

/* Print the operator of e as the source wrote it, ascii unless that was Unicode */
static void
PrettyOperator(Value f, Expr *e, const char *ascii)
{
    const char *utf8 = PrettyUtf8Operator(e);

    FilePuts(f, utf8 ? utf8 : ascii);
}

/*
 * Whether a power was written with its exponent in superscripts, as x²
 * or x⁻¹⁰, the exponent's text then holding its digits and any '-'
 */
static Bool
PrettySuperscript(Expr *e)
{
    Expr *exponent;

    if (e->base.tag != POW || !(e->base.flags & EXPR_UTF8))
        return False;
    exponent = e->tree.right;
    return exponent->base.tag == TEN_NUM && (exponent->base.flags & EXPR_UTF8)
        && exponent->constant.text;
}

static const char * const prettySuperscripts[]
    = { "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹" };

/* Print a power's exponent in superscripts, as written */
static void
PrettySuperscriptText(Value f, Expr *e)
{
    Value       text = e->tree.right->constant.text;
    const char *s = StringChars(&text->string);
    ptrdiff_t   i;

    for (i = 0; i < text->string.length; i++)
        FilePuts(f, s[i] == '-' ? "⁻" : prettySuperscripts[s[i] - '0']);
}

/*
 * Whether 'e' prints starting with a prefix operator, including a
 * negative constant's sign. A postfix operator binds more loosely than
 * these, so it needs parentheses around them, and a prefix minus needs
 * a space before one starting with '-'
 */
static Bool
PrettyPrefixed(Expr *e)
{
    switch (e->base.tag) {
    case UMINUS:
    case BANG:
    case LNOT:
    case STAR:
    case AMPER:
        return True;
    case INC:
    case DEC:
        return e->tree.left != 0;
    case TEN_NUM:
    case TEN_FLOAT:
    case OCTAL_NUM:
    case OCTAL0_NUM:
    case OCTAL_FLOAT:
    case OCTAL0_FLOAT:
    case BINARY_NUM:
    case BINARY_FLOAT:
    case HEX_NUM:
    case HEX_FLOAT:
        return Numericp(ValueTag(e->constant.constant)) && Negativep(e->constant.constant);
    default:
        return False;
    }
}

/* Whether 'e' prints starting with '-' */
static Bool
PrettyMinus(Expr *e)
{
    switch (e->base.tag) {
    case UMINUS:
        return True;
    case DEC:
        return e->tree.left != 0;
    case BANG:
    case LNOT:
    case STAR:
    case AMPER:
    case INC:
        return False;
    default:
        return PrettyPrefixed(e);
    }
}

/*
 * Print the operand of postfix '!', '++', or '--', which binds more
 * loosely than a prefix operator: (-a)! isn't -a!
 */
static void
PrettyPostfixOperand(Value f, Expr *e, int selfPrec, int level, Bool nest)
{
    if (PrettyPrefixed(e)) {
        FilePuts(f, "(");
        PrettyExpr(f, e, selfPrec, level, nest);
        FilePuts(f, ")");
    } else
        PrettyExpr(f, e, selfPrec, level, nest);
}

/*
 * The member access a chain of them continues with in e, the left side
 * of one, found through calls and subscripts, or 0
 */
static Expr *
PrettyNextMember(Expr *e)
{
    while (e && (e->base.tag == OP || e->base.tag == OS))
        e = e->tree.left;
    if (e && (e->base.tag == DOT || e->base.tag == ARROW))
        return e;
    return 0;
}

/*
 * Print an item of a comma-separated list, such as an argument or an
 * initializer, where only a comma expression needs parentheses
 */
static void
PrettyListItem(Value f, Expr *e, int level, Bool nest)
{
    PrettyExpr(f, e, PrettyPrecedence(COMMA) + 1, level, nest);
}

static void
PrettyParameters(Value f, Expr *e, int level, Bool nest)
{
    while (e) {
        if (e->tree.left->base.tag == DOTDOTDOT) {
            PrettyListItem(f, e->tree.left->tree.left, level, nest);
            PrettyOperator(f, e->tree.left, "...");
        } else {
            PrettyListItem(f, e->tree.left, level, nest);
        }
        e = e->tree.right;
        if (e) {
            FilePuts(f, ", ");
            PrettyMark(f, PRETTY_BREAK, 0);
        }
    }
}

static void PrettyCommentNote(Value f, CommentPtr comment);
static void PrettyNoteText(Value f, CommentPtr comment, Value one);
static void PrettyCommentList(Value f, Expr *c, CommentPlace place);
static void PrettyExprComments(Value f, Expr *e, CommentPlace place);
static void PrettyItemComments(Value f, Expr *e);

static void
PrettyArrayInits(Value f, Expr *e, int level, Bool nest)
{
    while (e) {
        if (e->tree.left)
            PrettyArrayInit(f, e->tree.left, level, nest);
        e = e->tree.right;
        if (e) {
            if (e->tree.left->base.tag == DOTDOTDOT)
                FilePuts(f, " ");
            else {
                FilePuts(f, ", ");
                PrettyMark(f, PRETTY_BREAK, 0);
            }
        }
    }
}

static void
PrettyArrayInit(Value f, Expr *e, int level, Bool nest)
{
    switch (e->base.tag) {
    case OC:
        if (nest)
            PrettyItemComments(f, e);
        FilePuts(f, "{ ");
        PrettyMark(f, PRETTY_GROUP, PRETTY_INITIALIZE);
        PrettyArrayInits(f, e->tree.left, level, nest);
        if (nest)
            PrettyExprComments(f, e, comment_closing);
        PrettyMark(f, PRETTY_END, 0);
        FilePuts(f, " }");
        if (nest)
            PrettyExprComments(f, e, comment_after);
        break;
    case DOTDOTDOT:
        PrettyOperator(f, e, "...");
        break;
    default:
        PrettyListItem(f, e, level, nest);
        break;
    }
}

/* Print an item of an initializer of kind tag: an array's, a hash's, or a struct's */
static void
PrettyInitItem(Value f, int tag, Expr *item, int level, Bool nest)
{
    switch (tag) {
    case HASH:
        if (nest)
            PrettyItemComments(f, item);
        /* A hash's default value has no key before its '=>' */
        if (item->tree.left) {
            PrettyListItem(f, item->tree.left, level, nest);
            FileOutput(f, ' ');
        }
        PrettyOperator(f, item, "=>");
        FileOutput(f, ' ');
        PrettyListItem(f, item->tree.right, level, nest);
        if (nest)
            PrettyExprComments(f, item, comment_after);
        break;
    case STRUCT:
        if (nest)
            PrettyItemComments(f, item);
        PrettyExpr(f, item->tree.left, -1, level, nest);
        FilePuts(f, " = ");
        PrettyListItem(f, item->tree.right, level, nest);
        if (nest)
            PrettyExprComments(f, item, comment_after);
        break;
    default:
        PrettyArrayInit(f, item, level, nest);
        break;
    }
}

static void
PrettyHashInit(Value f, Expr *e, int level, Bool nest)
{
    while (e) {
        PrettyInitItem(f, HASH, e->tree.left, level, nest);
        e = e->tree.right;
        if (e) {
            FilePuts(f, ", ");
            PrettyMark(f, PRETTY_BREAK, 0);
        }
    }
}

static void
PrettyStructInit(Value f, Expr *e, int level, Bool nest)
{
    while (e) {
        PrettyInitItem(f, STRUCT, e->tree.left, level, nest);
        e = e->tree.right;
        if (e) {
            FilePuts(f, ", ");
            PrettyMark(f, PRETTY_BREAK, 0);
        }
    }
}

/*
 * A comment's text on one line, or 0 when it doesn't lay out as one
 * line, as a comment printed inside an expression must
 */
static Value
PrettyCommentText(CommentPtr comment)
{
    ENTER();
    Value one = FileStringWrite();

    if (!PrettyCommentOneLine(one, comment->text, PRETTY_WIDTH))
        RETURN(0);
    RETURN(FileStringString(one));
}

/* Print a comment in an initializer's rows as a note the layout keeps whole */
static void
PrettyNote(Value f, CommentPtr comment, Value text)
{
    PrettyMark(f, PRETTY_GROUP, PRETTY_NOTE);
    FilePutsc(f, StringChars(&text->string), text->string.length);
    PrettyMark(f, PRETTY_END, 0);
    comment->printed = True;
}

/*
 * Where a comment before an item starting a row goes: on a line of its
 * own, at the end of the row before, or inline before the item
 */
typedef enum _prettyRowComment { pretty_own_line, pretty_row_end, pretty_inline } PrettyRowComment;

static PrettyRowComment
PrettyRowCommentPlace(CommentPtr comment, Bool after_row)
{
    if (!comment->ends_line)
        return pretty_inline;
    if (comment->starts_line || !after_row)
        return pretty_own_line;
    return pretty_row_end;
}

/*
 * Print the comments before an item that go where: on lines of their
 * own as rows of notes, ending the row before as notes, or inline.
 * Return whether a row now comes before the item
 */
static Bool
PrettyRowComments(Value f, Expr *item, Bool after_row, PrettyRowComment where)
{
    ENTER();
    Expr      *c;
    CommentPtr comment;
    Value      text;

    for (c = item->base.comments; c; c = c->tree.right) {
        comment = c->tree.left->comment.comment;
        if (comment->place != comment_before || PrettyRowCommentPlace(comment, after_row) != where
            || !(text = PrettyCommentText(comment)))
            continue;
        switch (where) {
        case pretty_own_line:
            if (after_row)
                PrettyMark(f, PRETTY_BREAK, 0);
            PrettyMark(f, PRETTY_GROUP, PRETTY_ROW);
            PrettyNote(f, comment, text);
            PrettyMark(f, PRETTY_END, 0);
            after_row = True;
            break;
        case pretty_row_end:
            FileOutput(f, ' ');
            PrettyNote(f, comment, text);
            break;
        case pretty_inline:
            PrettyNote(f, comment, text);
            FileOutput(f, ' ');
            break;
        }
    }
    EXIT();
    return after_row;
}

/*
 * Print the comments before an initializer's closing brace: those on
 * lines of their own, or else those ending the last row
 */
static void
PrettyClosingComments(Value f, Expr *e, Bool own_line)
{
    ENTER();
    Expr      *c;
    CommentPtr comment;
    Value      text;

    for (c = e->base.comments; c; c = c->tree.right) {
        comment = c->tree.left->comment.comment;
        if (comment->place != comment_closing || comment->starts_line != own_line
            || !(text = PrettyCommentText(comment)))
            continue;
        if (own_line) {
            PrettyMark(f, PRETTY_BREAK, 0);
            PrettyMark(f, PRETTY_GROUP, PRETTY_ROW);
            PrettyNote(f, comment, text);
            PrettyMark(f, PRETTY_END, 0);
        } else {
            FileOutput(f, ' ');
            PrettyNote(f, comment, text);
        }
    }
    EXIT();
}

/*
 * Print an initializer as rows, as written with a trailing comma: a
 * group of rows, each a group of items, every item followed by a comma.
 * Its comments keep their lines: one on a line of its own is a row of
 * its own, one ending a row's line ends the row, and others stay before
 * their items
 */
static void
PrettyRows(Value f, Expr *e, int level, Bool nest)
{
    Expr *items = e->tree.left;
    Bool  after_row = False;

    FilePuts(f, "{ ");
    PrettyMark(f, PRETTY_GROUP, PRETTY_ROWS);
    while (items) {
        if (nest)
            after_row = PrettyRowComments(f, items->tree.left, after_row, pretty_own_line);
        /* Each row starts a line */
        if (after_row)
            PrettyMark(f, PRETTY_BREAK, 0);
        PrettyMark(f, PRETTY_GROUP, PRETTY_ROW);
        for (;;) {
            Expr *item = items->tree.left;

            if (nest) {
                /* The rows print the comments before each item, not the item */
                PrettyRowComments(f, item, after_row && items != e->tree.left, pretty_inline);
                prettyHeld = item;
            }
            PrettyInitItem(f, e->base.tag, item, level, nest);
            prettyHeld = 0;
            FilePuts(f, ",");
            items = items->tree.right;
            if (!items || (items->base.flags & EXPR_ROW))
                break;
            /* The space leads the next item, so a line ending here doesn't count it */
            PrettyMark(f, PRETTY_BREAK, 0);
            FilePuts(f, " ");
        }
        if (nest) {
            /* The comments ending the row's line */
            if (items)
                PrettyRowComments(f, items->tree.left, True, pretty_row_end);
            else
                PrettyClosingComments(f, e, False);
        }
        PrettyMark(f, PRETTY_END, 0);
        after_row = True;
    }
    if (nest)
        PrettyClosingComments(f, e, True);
    PrettyMark(f, PRETTY_END, 0);
    FilePuts(f, " }");
}

/* Whether to print an initializer as rows, which only laying out code does */
static Bool
PrettyHasRows(Expr *e)
{
    return prettyMarking && (e->base.flags & EXPR_ROWS);
}

/* Print a character constant, escaping only what FilePutString escapes in strings */
static void
PrettyChar(Value f, uint32_t c)
{
    FileOutput(f, '\'');
    if (c < ' ')
        switch (c) {
        case '\n':
            FilePuts(f, "\\n");
            break;
        case '\r':
            FilePuts(f, "\\r");
            break;
        case '\b':
            FilePuts(f, "\\b");
            break;
        case '\t':
            FilePuts(f, "\\t");
            break;
        case '\f':
            FilePuts(f, "\\f");
            break;
        case '\v':
            FilePuts(f, "\\v");
            break;
        case '\0':
            FilePuts(f, "\\0");
            break;
        default:
            FileOutput(f, '\\');
            Print(f, NewCharInteger(c), 'o', 8, 3, -1, '0');
            break;
        }
    else if (c == '\'')
        FilePuts(f, "\\'");
    else if (c == '\\')
        FilePuts(f, "\\\\");
    else
        FileOutchar(f, c);
    FileOutput(f, '\'');
}

/* Whether a byte may continue a name or number */
static Bool
PrettyNameByte(unsigned char c)
{
    return c == '_' || c >= 0x80 || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')
        || ('0' <= c && c <= '9');
}

/*
 * The length of the token starting s, len bytes long, as the lexer
 * reads one in a type: a name or number, a quoted literal, or an
 * operator; 0 for a space, a comment, or a layout mark, whose length
 * goes in *skip
 */
static ptrdiff_t
PrettyTypeToken(const char *s, ptrdiff_t len, ptrdiff_t *skip)
{
    static const char * const ops[] = { "...", "**=", "<<=", ">>=", "::", "**", "&&", "||", "->",
                                        "<<",  ">>",  "<=",  ">=",  "==", "!=", "++", "--", "//",
                                        "^^",  "+=",  "-=",  "*=",  "/=", "%=", "&=", "|=", "^=" };
    ptrdiff_t                 i, n;
    unsigned char             c = (unsigned char)s[0];

    *skip = 0;
    /* Marks for laying out code, a group's with its kind */
    if (c == PRETTY_GROUP) {
        *skip = len > 1 ? 2 : 1;
        return 0;
    }
    if (c == ' ' || c == '\n' || c == '\t' || c == PRETTY_END || c == PRETTY_BREAK
        || c == PRETTY_NEWLINE) {
        *skip = 1;
        return 0;
    }
    if (c == '/' && len > 1 && s[1] == '*') {
        for (i = 2; i < len - 1 && !(s[i] == '*' && s[i + 1] == '/'); i++)
            ;
        *skip = i + 2 < len ? i + 2 : len;
        return 0;
    }
    /* A '#' in a type starts a member's comment, which runs to the end of its line */
    if (c == '#') {
        for (i = 1; i < len && s[i] != '\n'; i++)
            ;
        *skip = i;
        return 0;
    }
    if (PrettyNameByte(c)
        || (c == '.' && len > 1 && (s[1] == '{' || ('0' <= s[1] && s[1] <= '9')))) {
        Bool number = c == '.' || ('0' <= c && c <= '9');

        for (i = 1; i < len; i++) {
            c = (unsigned char)s[i];
            if (!PrettyNameByte(c) && !(number && (c == '.' || c == '{' || c == '}')))
                break;
        }
        return i;
    }
    if (c == '"' || c == '\'') {
        for (i = 1; i < len && s[i] != (char)c; i++)
            if (s[i] == '\\')
                i++;
        return i < len ? i + 1 : len;
    }
    for (i = 0; i < (ptrdiff_t)(sizeof(ops) / sizeof(ops[0])); i++) {
        n = (ptrdiff_t)strlen(ops[i]);
        if (n <= len && !strncmp(s, ops[i], (size_t)n))
            return n;
    }
    return 1;
}

/* Comment entries past the comment_type ones starting them, which print in the type */
static Expr *
PrettyPastType(Expr *c)
{
    while (c && c->tree.left && c->tree.left->comment.comment->place == comment_type)
        c = c->tree.right;
    if (c && !c->tree.left)
        return 0;
    return c;
}

/*
 * Print a type's text with the comment_type comments among comment
 * entries, each before as many of the text's tokens, counted from its
 * end, as followed it in the source, marking them printed
 */
static void
PrettyTypeText(Value f, Value text, Expr *comments)
{
    ENTER();
    const char *s = StringChars(&text->string);
    ptrdiff_t   len = text->string.length, n = 0, i, l, skip, k, at = 0, *starts;
    Expr       *c;

    for (c = comments; c && c->tree.left; c = c->tree.right)
        if (c->tree.left->comment.comment->place == comment_type)
            break;
    if (!c || !c->tree.left) {
        FilePutsc(f, s, len);
        EXIT();
        return;
    }
    for (i = 0; i < len; i += l ? l : skip)
        if ((l = PrettyTypeToken(s + i, len - i, &skip)))
            n++;
    starts = AllocateTemp((size_t)(n + 1) * sizeof(ptrdiff_t));
    for (i = 0, k = 0; i < len; i += l ? l : skip)
        if ((l = PrettyTypeToken(s + i, len - i, &skip)))
            starts[k++] = i;
    starts[n] = len;
    for (k = 0; k <= n; k++) {
        for (c = comments; c && c->tree.left; c = c->tree.right) {
            CommentPtr comment = c->tree.left->comment.comment;
            Value      one = FileStringWrite();
            ptrdiff_t  before = n - comment->after;
            char       last;

            if (comment->place != comment_type || (before > 0 ? before : 0) != k
                || !PrettyCommentOneLine(one, comment->text, PRETTY_WIDTH))
                continue;
            one = FileStringString(one);
            FilePutsc(f, s + at, starts[k] - at);
            at = starts[k];
            last = at ? s[at - 1] : ' ';
            if (last != ' ' && last != '\n' && last != '(' && last != '[')
                FileOutput(f, ' ');
            FilePutsc(f, StringChars(&one->string), one->string.length);
            if (k < n && !strchr(")],;", s[at]))
                FileOutput(f, ' ');
            comment->printed = True;
        }
    }
    FilePutsc(f, s + at, len - at);
    EXIT();
}

static void
PrettyDeclType(Value f, Expr *e, int level, Bool nest)
{
    ENTER();
    Value prefix = FileStringWrite();
    Value text = FileStringWrite();
    Type *type = 0;

    FilePutPublish(prefix, e->decl.publish, True);
    switch (e->decl.class) {
    case class_auto:
        if (e->decl.type)
            type = e->decl.type;
        else
            FilePuts(prefix, "auto");
        break;
    case class_const:
    case class_global:
    case class_static:
    case class_typedef:
    case class_namespace:
        FilePutClass(prefix, e->decl.class, e->decl.type && !TypePoly(e->decl.type));
        if (e->decl.type && !TypePoly(e->decl.type))
            type = e->decl.type;
        break;
    case class_undef:
    case class_arg:
        type = e->decl.type;
        break;
    case class_exception:
        FilePutClass(prefix, e->decl.class, False);
        break;
    }
    prefix = FileStringString(prefix);
    FilePutsc(text, StringChars(&prefix->string), prefix->string.length);
    /* A struct or union type lays out as PrettyType does */
    if (type)
        PrettyType(text, type, level, nest, (ptrdiff_t)level * 4 + PrettyTextColumns(prefix), 0);
    PrettyTypeText(f, FileStringString(text), nest ? e->base.comments : 0);
    EXIT();
}

/*
 * Print a declared name, with the comments around it when printing
 * nested; one starting a line, as in the source, takes only those
 * starting the name's line, PrettyDeclBreak printing the rest
 */
static void
PrettyDeclName(Value f, DeclListPtr decl, Bool nest, Bool starts)
{
    Expr      *c;
    CommentPtr comment;

    if (nest)
        for (c = decl->comments; c; c = c->tree.right) {
            comment = c->tree.left->comment.comment;
            if (comment->place == comment_before
                && (!starts || (comment->starts_line && !comment->ends_line)))
                PrettyCommentNote(f, comment);
        }
    FilePuts(f, AtomName(decl->name));
    if (nest)
        PrettyCommentList(f, decl->comments, comment_after);
}

/* Whether a comment ending its line follows a declared name */
static Bool
PrettyDeclNameEndsLine(DeclListPtr decl)
{
    Expr *c;

    for (c = decl->comments; c; c = c->tree.right)
        if (c->tree.left->comment.comment->place == comment_after
            && c->tree.left->comment.comment->ends_line)
            return True;
    return False;
}

/*
 * Whether a declaration breaks where its source did: before a name
 * after its ',', or after a comment ending the line between a name and
 * its '='
 */
static Bool
PrettyDeclBreaks(Expr *e, Bool nest)
{
    DeclListPtr decl;

    if (!nest)
        return False;
    for (decl = e->decl.decl; decl; decl = decl->next)
        if ((decl != e->decl.decl && decl->starts_line) || PrettyDeclNameEndsLine(decl))
            return True;
    return False;
}

/*
 * Break the line before a name, as the source did, at column names:
 * the comments after the ',' end its line, and those on lines of their
 * own keep them
 */
static void
PrettyDeclBreak(Value f, DeclListPtr decl, ptrdiff_t names)
{
    ENTER();
    Expr      *c;
    CommentPtr comment;
    Value      one;
    Bool       own;
    ptrdiff_t  i;

    for (own = False;; own = True) {
        for (c = decl->comments; c; c = c->tree.right) {
            comment = c->tree.left->comment.comment;
            one = FileStringWrite();
            if (comment->place != comment_before || comment->starts_line != own
                || (own && !comment->ends_line)
                || !PrettyCommentOneLine(one, comment->text, PRETTY_WIDTH))
                continue;
            if (own) {
                FileOutput(f, '\n');
                for (i = 0; i < names; i++)
                    FileOutput(f, ' ');
            } else
                FileOutput(f, ' ');
            PrettyNoteText(f, comment, FileStringString(one));
        }
        if (own)
            break;
    }
    FileOutput(f, '\n');
    for (i = 0; i < names; i++)
        FileOutput(f, ' ');
    EXIT();
}

/* Print the names a declaration declares, with their initializers */
static void
PrettyDeclNames(Value f, Expr *e, int level, Bool nest)
{
    DeclListPtr decl;

    for (decl = e->decl.decl; decl; decl = decl->next) {
        PrettyDeclName(f, decl, nest, False);
        if (decl->init) {
            FilePuts(f, " = ");
            PrettyListItem(f, decl->init, level, nest);
        }
        if (decl->next)
            FilePuts(f, ", ");
    }
}

/*
 * How well printed text fits starting at column col, with tail more
 * columns after its last line: the lines it takes, and how many of
 * them pass the end of the line
 */
static void
PrettyTextFit(Value text, ptrdiff_t col, ptrdiff_t tail, ptrdiff_t *lines, ptrdiff_t *over)
{
    const char *s = StringChars(&text->string);
    ptrdiff_t   len = text->string.length, i, cols = col;

    *lines = 1;
    *over = 0;
    for (i = 0; i <= len; i++) {
        if (i == len || s[i] == '\n') {
            if (cols + (i == len ? tail : 0) > PRETTY_LINE_END)
                ++*over;
            if (i < len)
                ++*lines;
            cols = 0;
        } else if ((s[i] & 0xc0) != 0x80)
            cols++;
    }
}

/*
 * Whether printed text's first line holds just a comment, which ended
 * its line in the source: moving the text to the next line would start
 * a line with it
 */
static Bool
PrettyNoteLine(Value text)
{
    const char *s = StringChars(&text->string);
    ptrdiff_t   len = text->string.length, i;

    for (i = 0; i < len && s[i] != '\n'; i++)
        ;
    return i >= 4 && i < len && !strncmp(s, "/*", 2) && !strncmp(s + i - 2, "*/", 2);
}

/* The columns a declared name and its initializer take on one line */
static ptrdiff_t
PrettyDeclNameWidth(DeclListPtr decl, int level, Bool nest)
{
    ENTER();
    Bool      was = prettyMarking;
    Value     name = FileStringWrite(), init;
    ptrdiff_t width;

    prettyMarking = False;
    PrettyDeclName(name, decl, nest, False);
    name = FileStringString(name);
    width = PrettyTextColumns(name);
    if (decl->init) {
        init = FileStringWrite();
        PrettyExpr(init, decl->init, PrettyPrecedence(COMMA) + 1, level, nest);
        init = FileStringString(init);
        width += 3 + PrettyFlatWidth(StringChars(&init->string), init->string.length);
    }
    prettyMarking = was;
    EXIT();
    return width;
}

/* The columns a declaration's names and initializers take on one line */
static ptrdiff_t
PrettyDeclNamesWidth(Expr *e, int level, Bool nest)
{
    DeclListPtr decl;
    ptrdiff_t   width = 0;

    for (decl = e->decl.decl; decl; decl = decl->next) {
        if (decl != e->decl.decl)
            width += 2;
        width += PrettyDeclNameWidth(decl, level, nest);
    }
    return width;
}

/*
 * Print the names a declaration declares, starting at column col, with
 * their initializers broken to fit as code; a name that starts a line
 * in the source, or doesn't fit after the last, starts a line under
 * the first, and a comment ending the line after a name puts its '='
 * on the next line, four columns further in. paren says the global
 * declaration is in parentheses, where a newline doesn't end it
 */
static void
PrettyWrapDeclNames(Value f, Expr *e, int level, Bool nest, ptrdiff_t col, Bool paren)
{
    ENTER();
    DeclListPtr decl;
    ptrdiff_t   names = col, end = PrettyCodeEnd((ptrdiff_t)level * 4), i;
    Bool        own_line, breaks = nest && (level > 0 || paren);

    for (decl = e->decl.decl; decl; decl = decl->next) {
        Value     name = FileStringWrite();
        /* The ',' or ';' after it, and the ')' closing a global declaration */
        ptrdiff_t tail = decl->next || !paren ? 1 : 2;
        Bool      starts = breaks && decl != e->decl.decl && decl->starts_line;
        Bool      assign = breaks && decl->init && PrettyDeclNameEndsLine(decl);

        PrettyDeclName(name, decl, nest, starts);
        name = FileStringString(name);

        if (starts) {
            FileOutput(f, ',');
            PrettyDeclBreak(f, decl, names);
            col = names;
        } else if (decl != e->decl.decl) {
            /*
             * A name that doesn't fit after the last starts a line under
             * the first, where a newline doesn't end a global statement
             */
            if ((level > 0 || paren)
                && col + 2 + PrettyDeclNameWidth(decl, level, nest) + tail > end) {
                FilePuts(f, ",\n");
                for (i = 0; i < names; i++)
                    FileOutput(f, ' ');
                col = names;
            } else {
                FilePuts(f, ", ");
                col += 2;
            }
        }
        FilePutsc(f, StringChars(&name->string), name->string.length);
        col += PrettyTextColumns(name);
        if (decl->init) {
            Value init = FileStringWrite();

            if (assign) {
                FileOutput(f, '\n');
                for (i = 0; i < names + 4; i++)
                    FileOutput(f, ' ');
                FilePuts(f, "=");
                col = names + 6;
            } else {
                FilePuts(f, " =");
                col += 3;
            }
            /* The ',' or ';' after it follows on its last line */
            PrettyWrapExpr(init, decl->init, PrettyPrecedence(COMMA) + 1, level, nest, col, 0, tail,
                           paren);
            init = FileStringString(init);
            /*
             * In a function, or in a global declaration's parentheses, a
             * value goes on the next line, four columns in, when it fits
             * on one line there and not after the '=', as an assignment's
             * does, or when that fits better, with fewer lines past the
             * end of the line, or as many and fewer lines in all
             */
            own_line = False;
            if ((level > 0 || paren) && nest && !assign && (ptrdiff_t)level * 4 + 4 < col
                && (init->string.length == 0 || StringChars(&init->string)[0] != '\n')
                && !PrettyNoteLine(init)) {
                ptrdiff_t moved_col = (ptrdiff_t)level * 4 + 4;
                Value     moved = FileStringWrite();
                ptrdiff_t lines, over, moved_lines, moved_over;
                Bool      flat, moved_flat;

                PrettyWrapExpr(moved, decl->init, PrettyPrecedence(COMMA) + 1, level, nest,
                               moved_col, 0, tail, paren);
                moved = FileStringString(moved);
                PrettyTextFit(init, col, tail, &lines, &over);
                PrettyTextFit(moved, moved_col, tail, &moved_lines, &moved_over);
                flat = !PrettyMultiline(init) && col + PrettyTextColumns(init) + tail <= end;
                moved_flat
                    = !PrettyMultiline(moved) && moved_col + PrettyTextColumns(moved) + tail <= end;
                /* Moving adds the line the '=' ends */
                if ((moved_flat && !flat) || moved_over < over
                    || (moved_over == over && moved_lines + 1 < lines)) {
                    FileOutput(f, '\n');
                    for (i = 0; i < moved_col; i++)
                        FileOutput(f, ' ');
                    init = moved;
                    col = moved_col;
                    own_line = True;
                }
            }
            /* A value can start a line of its own, as a lambda does */
            if (!own_line && (init->string.length == 0 || StringChars(&init->string)[0] != '\n'))
                FileOutput(f, ' ');
            FilePutsc(f, StringChars(&init->string), init->string.length);
            if (PrettyMultiline(init))
                col = PrettyTextColumns(init);
            else
                col += PrettyTextColumns(init);
        }
    }
    EXIT();
}

static void
PrettyDecl(Value f, Expr *e, int level, Bool nest)
{
    PrettyDeclType(f, e, level, nest);
    if (e->decl.decl) {
        FileOutput(f, ' ');
        PrettyDeclNames(f, e, level, nest);
    }
    if (e->decl.class == class_exception) {
        FilePutArgType(f, e->decl.type->func.args);
    }
}

/* Whether tag is an assignment operator */
static Bool
PrettyAssignment(int tag)
{
    switch (tag) {
    case ASSIGN:
    case ASSIGNPLUS:
    case ASSIGNMINUS:
    case ASSIGNTIMES:
    case ASSIGNDIVIDE:
    case ASSIGNMOD:
    case ASSIGNDIV:
    case ASSIGNPOW:
    case ASSIGNSHIFTL:
    case ASSIGNSHIFTR:
    case ASSIGNLXOR:
    case ASSIGNLAND:
    case ASSIGNLOR:
    case ASSIGNAND:
    case ASSIGNOR:
        return True;
    default:
        return False;
    }
}

/* Print the binary operator of e with the spaces around it */
static void
PrettyBinaryOperator(Value f, Expr *e)
{
    const char *utf8 = PrettyUtf8Operator(e);

    if (utf8) {
        FileOutput(f, ' ');
        FilePuts(f, utf8);
        FileOutput(f, ' ');
        return;
    }
    switch (e->base.tag) {
    case PLUS:
        FilePuts(f, " + ");
        break;
    case MINUS:
        FilePuts(f, " - ");
        break;
    case TIMES:
        FilePuts(f, " * ");
        break;
    case DIVIDE:
        FilePuts(f, " / ");
        break;
    case MOD:
        FilePuts(f, " % ");
        break;
    case DIV:
        FilePuts(f, " // ");
        break;
    case POW:
        FilePuts(f, " ** ");
        break;
    case SHIFTL:
        FilePuts(f, " << ");
        break;
    case SHIFTR:
        FilePuts(f, " >> ");
        break;
    case LXOR:
        FilePuts(f, " ^ ");
        break;
    case LAND:
        FilePuts(f, " & ");
        break;
    case LOR:
        FilePuts(f, " | ");
        break;
    case EQ:
        FilePuts(f, " == ");
        break;
    case NE:
        FilePuts(f, " != ");
        break;
    case LT:
        FilePuts(f, " < ");
        break;
    case GT:
        FilePuts(f, " > ");
        break;
    case LE:
        FilePuts(f, " <= ");
        break;
    case GE:
        FilePuts(f, " >= ");
        break;
    case AND:
        FilePuts(f, " && ");
        break;
    case OR:
        FilePuts(f, " || ");
        break;
    case ASSIGN:
        FilePuts(f, " = ");
        break;
    case ASSIGNPLUS:
        FilePuts(f, " += ");
        break;
    case ASSIGNMINUS:
        FilePuts(f, " -= ");
        break;
    case ASSIGNTIMES:
        FilePuts(f, " *= ");
        break;
    case ASSIGNDIVIDE:
        FilePuts(f, " /= ");
        break;
    case ASSIGNMOD:
        FilePuts(f, " %= ");
        break;
    case ASSIGNDIV:
        FilePuts(f, " //= ");
        break;
    case ASSIGNPOW:
        FilePuts(f, " **= ");
        break;
    case ASSIGNSHIFTL:
        FilePuts(f, " <<= ");
        break;
    case ASSIGNSHIFTR:
        FilePuts(f, " >>= ");
        break;
    case ASSIGNLXOR:
        FilePuts(f, " ^= ");
        break;
    case ASSIGNLAND:
        FilePuts(f, " &= ");
        break;
    case ASSIGNLOR:
        FilePuts(f, " |= ");
        break;
    case ASSIGNAND:
        FilePuts(f, " &&= ");
        break;
    case ASSIGNOR:
        FilePuts(f, " ||= ");
        break;
    case COMMA:
        FilePuts(f, ", ");
        break;
    }
}

/* Print a comment's text laid out on one line, as a note, marking it printed */
static void
PrettyNoteText(Value f, CommentPtr comment, Value one)
{
    PrettyMark(f, PRETTY_GROUP, PRETTY_NOTE);
    FilePutsc(f, StringChars(&one->string), one->string.length);
    PrettyMark(f, PRETTY_END, 0);
    comment->printed = True;
}

/*
 * Print a comment an expression or a declared name holds before or
 * after it, inline when its text fits on one line, marking it printed.
 * One that started or ended its line in the source starts or ends a
 * line here
 */
static void
PrettyCommentNote(Value f, CommentPtr comment)
{
    ENTER();
    Value one = FileStringWrite();
    Bool  before = comment->place == comment_before || comment->place == comment_inside_before;

    if (!PrettyCommentOneLine(one, comment->text, PRETTY_WIDTH)) {
        EXIT();
        return;
    }
    /* One on a line of its own starts one */
    if (before && comment->starts_line && comment->ends_line)
        PrettyMark(f, PRETTY_NEWLINE, 0);
    else if (!before)
        FileOutput(f, ' ');
    PrettyNoteText(f, comment, FileStringString(one));
    if (comment->ends_line && prettyMarking)
        PrettyMark(f, PRETTY_NEWLINE, 0);
    else if (before)
        FileOutput(f, ' ');
    EXIT();
}

/* Print the comment entries in a place */
static void
PrettyCommentList(Value f, Expr *c, CommentPlace place)
{
    for (; c; c = c->tree.right)
        if (c->tree.left->comment.comment->place == place)
            PrettyCommentNote(f, c->tree.left->comment.comment);
}

static void
PrettyExprComments(Value f, Expr *e, CommentPlace place)
{
    PrettyCommentList(f, e->base.comments, place);
}

/* Print the comments before an item, unless its initializer prints them */
static void
PrettyItemComments(Value f, Expr *e)
{
    if (e == prettyHeld)
        prettyHeld = 0;
    else
        PrettyExprComments(f, e, comment_before);
}

static void PrettyExprText(Value f, Expr *e, int parentPrec, int level, Bool nest);

/*
 * Print an initializer's type in parentheses, with the comments among
 * its tokens, and a struct's or union's members' comments, as
 * PrettyType lays it out
 */
static void
PrettyNewTypeText(Value f, Expr *e, int level, Bool nest)
{
    ENTER();
    Value type = FileStringWrite();

    PrettyType(type, e->base.type, level, nest, (ptrdiff_t)level * 4, 0);
    FileOutput(f, '(');
    PrettyTypeText(f, FileStringString(type), nest ? e->base.comments : 0);
    FileOutput(f, ')');
    EXIT();
}

/*
 * Mark a lambda with a block, or an initializer's type spanning lines,
 * as its first line, for measuring, and its number in prettyShifted;
 * laying the text out prints it where it lands
 */
static void
PrettyMarkShifted(Value f, Expr *e, Value text)
{
    const char *t = StringChars(&text->string);
    ptrdiff_t   n;

    for (n = 0; n < text->string.length && t[n] != '\n'; n++)
        ;
    if (prettyShiftedCount == prettyShiftedSize) {
        prettyShiftedSize = prettyShiftedSize ? prettyShiftedSize * 2 : 16;
        prettyShifted = realloc(prettyShifted, (size_t)prettyShiftedSize * sizeof(Expr *));
        if (!prettyShifted)
            MemExhausted();
    }
    prettyShifted[prettyShiftedCount] = e;
    PrettyMark(f, PRETTY_GROUP, PRETTY_SHIFTED);
    FilePutsc(f, t, n);
    FilePrintf(f, "\n%z", prettyShiftedCount++);
    PrettyMark(f, PRETTY_END, 0);
}

/*
 * Print an initializer's type; one spanning lines, a struct or union
 * with its members on lines of their own, goes where laying the text
 * out places it, its members four columns past its start
 */
static void
PrettyNewType(Value f, Expr *e, int level, Bool nest)
{
    ENTER();
    Value text;

    if (prettyMarking && nest) {
        text = FileStringWrite();
        prettyMarking = False;
        PrettyNewTypeText(text, e, 0, nest);
        prettyMarking = True;
        text = FileStringString(text);
        if (PrettyMultiline(text)) {
            PrettyMarkShifted(f, e, text);
            EXIT();
            return;
        }
    }
    PrettyNewTypeText(f, e, level, nest);
    EXIT();
}

/* Print an expression, with the comments it holds when printing nested */
void
PrettyExpr(Value f, Expr *e, int parentPrec, int level, Bool nest)
{
    if (e && nest && e->base.comments) {
        PrettyItemComments(f, e);
        PrettyExprText(f, e, parentPrec, level, nest);
        PrettyExprComments(f, e, comment_after);
    } else
        PrettyExprText(f, e, parentPrec, level, nest);
}

/*
 * Print the operands of a comma expression, which groups to the left,
 * each but the last followed by ',' and a break; a declaration before
 * the last is parenthesized, as it would take the operands after it as
 * more names
 */
static void
PrettyCommaOperands(Value f, Expr *e, int level, Bool nest)
{
    Expr *left = e->tree.left;
    int   prec = PrettyPrecedence(COMMA);

    if (left->base.tag == COMMA && !(nest && left->base.comments))
        PrettyCommaOperands(f, left, level, nest);
    else if (left->base.tag == VAR)
        PrettyExpr(f, left, prec + 1, level, nest);
    else
        PrettyExpr(f, left, prec, level, nest);
    FilePuts(f, ",");
    PrettyMark(f, PRETTY_BREAK, 0);
    FilePuts(f, " ");
    PrettyExpr(f, e->tree.right, prec + 1, level, nest);
}

static void
PrettyExprText(Value f, Expr *e, int parentPrec, int level, Bool nest)
{
    int   selfPrec, leftPrec, rightPrec;
    Bool  clarifyLeft;
    Expr *node = e; /* the node, as e becomes the one ** wraps */

    if (!e)
        return;
    selfPrec = PrettyPrecedence(e->base.tag);
    if (selfPrec < parentPrec)
        FilePuts(f, "(");
    if (nest && e->base.comments)
        PrettyExprComments(f, e, comment_inside_before);
    switch (e->base.tag) {
    case NAME:
        FilePuts(f, AtomName(e->atom.atom));
        break;
    case VAR:
        PrettyDecl(f, e, level, nest);
        break;
    case OP:
        PrettyExpr(f, e->tree.left, selfPrec, level, nest);
        FilePuts(f, "(");
        if (e->tree.right) {
            PrettyMark(f, PRETTY_GROUP, PRETTY_CALL);
            PrettyParameters(f, e->tree.right, level, nest);
            PrettyMark(f, PRETTY_END, 0);
        }
        if (nest)
            PrettyExprComments(f, e, comment_closing);
        FilePuts(f, ")");
        break;
    case OS:
        PrettyExpr(f, e->tree.left, selfPrec, level, nest);
        FilePuts(f, "[");
        /* Subscripts break as call arguments do, as a last resort */
        PrettyMark(f, PRETTY_GROUP, PRETTY_SUBSCRIPT);
        PrettyParameters(f, e->tree.right, level, nest);
        PrettyMark(f, PRETTY_END, 0);
        FilePuts(f, "]");
        break;
    case NEW:
        /* A type in parentheses needs an initializer; {} leaves the value uninitialized */
        if (e->tree.left) {
            /* The value may start the next line */
            PrettyMark(f, PRETTY_GROUP, PRETTY_CAST);
            PrettyNewType(f, e, level, nest);
            FileOutput(f, ' ');
            PrettyMark(f, PRETTY_BREAK, 0);
            PrettyExpr(f, e->tree.left, selfPrec, level, nest);
            PrettyMark(f, PRETTY_END, 0);
        } else {
            PrettyNewType(f, e, level, nest);
            FilePuts(f, " {}");
        }
        break;
    case ARRAY:
        if (PrettyHasRows(e)) {
            PrettyRows(f, e, level, nest);
            break;
        }
        FilePuts(f, "{ ");
        PrettyMark(f, PRETTY_GROUP, PRETTY_INITIALIZE);
        PrettyArrayInits(f, e->tree.left, level, nest);
        if (nest)
            PrettyExprComments(f, e, comment_closing);
        PrettyMark(f, PRETTY_END, 0);
        FilePuts(f, " }");
        break;
    case COMP: {
        Expr *names = e->tree.left->tree.left;

        FilePuts(f, "{ ");
        FilePuts(f, "[");
        /* The index names are a list, which a comma expression would group the other way */
        for (; names->base.tag == COMMA; names = names->tree.right) {
            PrettyExpr(f, names->tree.left, -1, level, nest);
            FilePuts(f, ", ");
        }
        PrettyExpr(f, names, -1, level, nest);
        FilePuts(f, "] ");
        if (e->tree.right->base.tag == OC) {
            /* The block opens on this line and closes at the statement's level */
            FilePuts(f, "{\n");
            PrettyBlock(f, e->tree.right, level + 1, nest, True);
            PrettyIndent(f, 0, level);
            FilePuts(f, "}");
        } else {
            FilePuts(f, "= ");
            PrettyListItem(f, e->tree.right, level, nest);
        }
        FilePuts(f, " }");
        break;
    }
    case HASH:
        if (PrettyHasRows(e)) {
            PrettyRows(f, e, level, nest);
            break;
        }
        FilePuts(f, "{ ");
        PrettyMark(f, PRETTY_GROUP, PRETTY_INITIALIZE);
        PrettyHashInit(f, e->tree.left, level, nest);
        if (nest)
            PrettyExprComments(f, e, comment_closing);
        PrettyMark(f, PRETTY_END, 0);
        FilePuts(f, " }");
        break;
    case ANONINIT:
        FilePuts(f, "{}");
        break;
    case STRUCT:
        if (PrettyHasRows(e)) {
            PrettyRows(f, e, level, nest);
            break;
        }
        FilePuts(f, "{ ");
        PrettyMark(f, PRETTY_GROUP, PRETTY_INITIALIZE);
        PrettyStructInit(f, e->tree.left, level, nest);
        if (nest)
            PrettyExprComments(f, e, comment_closing);
        PrettyMark(f, PRETTY_END, 0);
        FilePuts(f, " }");
        break;
    case UNION:
        if (e->tree.right) {
            PrettyMark(f, PRETTY_GROUP, PRETTY_CAST);
            FilePrintf(f, "(%T.%A) ", e->base.type, e->tree.left->atom.atom);
            PrettyMark(f, PRETTY_BREAK, 0);
            PrettyExpr(f, e->tree.right, selfPrec, level, nest);
            PrettyMark(f, PRETTY_END, 0);
        } else {
            FilePrintf(f, "%T.%A", e->base.type, e->tree.left->atom.atom);
        }
        break;
    /* Precision -2 prints every digit the exact constant needs */
    case OCTAL_NUM:
    case OCTAL0_NUM:
    case OCTAL_FLOAT:
    case OCTAL0_FLOAT:
    case BINARY_NUM:
    case BINARY_FLOAT:
    case HEX_NUM:
    case HEX_FLOAT:
        if (e->constant.text) {
            FilePutsc(f, StringChars(&e->constant.text->string), e->constant.text->string.length);
            break;
        }
        switch (e->base.tag) {
        case BINARY_NUM:
        case BINARY_FLOAT:
            FilePrintf(f, "0b");
            Print(f, e->constant.constant, 'b', 2, 0, -2, ' ');
            break;
        case HEX_NUM:
        case HEX_FLOAT:
            FilePrintf(f, "0x");
            Print(f, e->constant.constant, 'x', 16, 0, -2, ' ');
            break;
        default:
            FilePrintf(f, "0o");
            Print(f, e->constant.constant, 'o', 8, 0, -2, ' ');
            break;
        }
        break;
    case POLY_CONST:
        /*
         * A command statement calls its function as a constant; print
         * the command's name rather than the function's whole text
         */
        if (ValueIsFunc(e->constant.constant)) {
            CommandPtr c;

            for (c = CurrentCommands; c; c = c->previous)
                if (c->func == e->constant.constant)
                    break;
            if (c) {
                FilePuts(f, AtomName(c->name));
                break;
            }
        }
        FilePrintf(f, "%v", e->constant.constant);
        break;
    case TEN_NUM:
    case TEN_FLOAT:
        /* A number prints as written */
        if (e->constant.text)
            FilePutsc(f, StringChars(&e->constant.text->string), e->constant.text->string.length);
        else
            FilePrintf(f, "%v", e->constant.constant);
        break;
    case VOIDVAL:
        PrettyOperator(f, e, "<>");
        break;
    case STRING_CONST:
    case THREAD_CONST:
    case BOOLVAL:
        FilePrintf(f, "%v", e->constant.constant);
        break;
    case CHAR_CONST:
        PrettyChar(f, CharPart(e->constant.constant, "malformed character constant"));
        break;
    case FUNC:
        /* A body printed as '= expr' takes the operators after it, so it needs parentheses then */
        if (nest && parentPrec > PrettyPrecedence(ASSIGN)
            && PrettyShortBody(e->code.code, 0, 0, level)) {
            FilePuts(f, "(");
            PrettyCode(f, e->code.code, 0, class_undef, publish_private, level, nest);
            FilePuts(f, ")");
        } else if (prettyMarking && !PrettyShortBody(e->code.code, 0, 0, level)) {
            /* A lambda with a block prints where it lands */
            CodePtr code = e->code.code;
            Value   header = FileStringWrite();

            prettyMarking = False;
            FilePrintf(header, "%tfunc", code->base.type);
            PrettyArgs(header, code->base.args, 0, True);
            FilePuts(header, " {");
            prettyMarking = True;
            PrettyMarkShifted(f, e, FileStringString(header));
        } else
            PrettyCode(f, e->code.code, 0, class_undef, publish_private, level, nest);
        break;
    case POW:
    case ASSIGNPOW:
        e = e->tree.right;
        if (PrettySuperscript(e)) {
            /* x² binds as x ** 2, so a power as its operand needs parentheses */
            PrettyExpr(f, e->tree.left, selfPrec + 1, level, nest);
            PrettySuperscriptText(f, e);
            break;
        }
        __NICKLE_FALLTHROUGH;
    case PLUS:
    case MINUS:
    case TIMES:
    case DIVIDE:
    case MOD:
    case DIV:
    case SHIFTL:
    case SHIFTR:
    case LXOR:
    case LAND:
    case LOR:
    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
    case AND:
    case OR:
    case ASSIGN:
    case ASSIGNPLUS:
    case ASSIGNMINUS:
    case ASSIGNTIMES:
    case ASSIGNDIVIDE:
    case ASSIGNMOD:
    case ASSIGNDIV:
    case ASSIGNSHIFTL:
    case ASSIGNSHIFTR:
    case ASSIGNLXOR:
    case ASSIGNLAND:
    case ASSIGNLOR:
    case ASSIGNAND:
    case ASSIGNOR:
    case COMMA:
        /*
         * Parenthesize an operand of the same precedence only on the
         * side the operator doesn't group toward: a - (b - c), but
         * a - b - c, and (a ** b) ** c, but a ** b ** c
         */
        leftPrec = rightPrec = selfPrec;
        if (PrettyRightAssoc(e->base.tag))
            leftPrec++;
        else
            rightPrec++;
        /* Some operands keep parentheses to show how they group */
        clarifyLeft = PrettyClarifies(e->base.tag, e->tree.left->base.tag);
        if (clarifyLeft)
            leftPrec = PrettyPrecedence(e->tree.left->base.tag) + 1;
        if (PrettyClarifies(e->base.tag, e->tree.right->base.tag))
            rightPrec = PrettyPrecedence(e->tree.right->base.tag) + 1;
        /* A declaration would take the operands after it as more names */
        if (e->base.tag == COMMA && e->tree.left->base.tag == VAR)
            leftPrec++;
        if (e->base.tag == COMMA && prettyMarking) {
            /* A comma expression's operands make a group, which breaks after each ',' */
            PrettyMark(f, PRETTY_GROUP, PRETTY_COMMAS);
            PrettyCommaOperands(f, e, level, nest);
            PrettyMark(f, PRETTY_END, 0);
            break;
        }
        {
            /*
             * Operators of one precedence above assignment make a
             * group, which breaks before each operator; a left operand
             * of the same precedence continues the group
             */
            Bool chain = prettyMarking && selfPrec > PrettyPrecedence(ASSIGN)
                && selfPrec != PrettyPrecedence(QUEST);
            Bool continuing = chain && e == prettyChain;
            /* A statement's assignment makes a group, its value after the break */
            Bool assign = prettyMarking && e == prettyAssign;

            if (chain && !continuing)
                PrettyMark(f, PRETTY_GROUP, (char)(PRETTY_OPERATORS + selfPrec));
            if (assign) {
                PrettyMark(f, PRETTY_GROUP, PRETTY_ASSIGN);
                prettyAssign = 0;
            }
            if (chain && !PrettyRightAssoc(e->base.tag) && !clarifyLeft
                && PrettyPrecedence(e->tree.left->base.tag) == selfPrec)
                prettyChain = e->tree.left;
            PrettyExpr(f, e->tree.left, leftPrec, level, nest);
            if (chain)
                PrettyMark(f, PRETTY_BREAK, 0);
            PrettyBinaryOperator(f, e);
            if (assign)
                PrettyMark(f, PRETTY_BREAK, 0);
            PrettyExpr(f, e->tree.right, rightPrec, level, nest);
            if (chain && !continuing)
                PrettyMark(f, PRETTY_END, 0);
            if (assign)
                PrettyMark(f, PRETTY_END, 0);
        }
        break;
    case FACT:
        PrettyPostfixOperand(f, e->tree.right, selfPrec, level, nest);
        FilePuts(f, "!");
        break;
    case LNOT:
    case UMINUS:
    case BANG:
    case INC:
    case DEC:
        if (e->tree.right)
            PrettyPostfixOperand(f, e->tree.right, selfPrec, level, nest);
        switch (e->base.tag) {
        case LNOT:
            PrettyOperator(f, e, "~");
            break;
        case UMINUS:
            FilePuts(f, "-");
            break;
        case BANG:
            FilePuts(f, "!");
            break;
        case INC:
            PrettyOperator(f, e, "++");
            break;
        case DEC:
            FilePuts(f, "--");
            break;
        }
        if (e->tree.left) {
            /* Keep - -a and - --a from reading as -- and --- */
            if ((e->base.tag == UMINUS || e->base.tag == DEC) && PrettyMinus(e->tree.left))
                FilePuts(f, " ");
            PrettyExpr(f, e->tree.left, selfPrec, level, nest);
        }
        break;
    case STAR:
        FilePuts(f, "*");
        PrettyExpr(f, e->tree.left, selfPrec, level, nest);
        break;
    case AMPER:
        FilePuts(f, "&");
        PrettyExpr(f, e->tree.left, selfPrec, level, nest);
        break;
    case COLONCOLON:
        PrettyExpr(f, e->tree.left, selfPrec, level, nest);
        FilePuts(f, "::");
        PrettyExpr(f, e->tree.right, selfPrec, level, nest);
        break;
    case DOT:
    case ARROW: {
        /*
         * A chain of member accesses, through calls and subscripts, makes
         * a group, which breaks before each '.' or '->'
         */
        Bool chain = prettyMarking, continuing = chain && e == prettyMembers;

        if (chain && !continuing)
            PrettyMark(f, PRETTY_GROUP, PRETTY_MEMBERS);
        if (chain)
            prettyMembers = PrettyNextMember(e->tree.left);
        PrettyExpr(f, e->tree.left, selfPrec, level, nest);
        PrettyMark(f, PRETTY_BREAK, 0);
        if (e->base.tag == DOT)
            FilePuts(f, ".");
        else
            PrettyOperator(f, e, "->");
        PrettyExpr(f, e->tree.right, selfPrec, level, nest);
        if (chain && !continuing)
            PrettyMark(f, PRETTY_END, 0);
        break;
    }
    case QUEST:
        /* ?: groups to the right, so a condition of the same precedence needs parentheses */
        PrettyMark(f, PRETTY_GROUP, PRETTY_CONDITION);
        PrettyExpr(f, e->tree.left, selfPrec + 1, level, nest);
        PrettyMark(f, PRETTY_BREAK, 0);
        FilePuts(f, " ? ");
        PrettyExpr(f, e->tree.right->tree.left, selfPrec, level, nest);
        PrettyMark(f, PRETTY_BREAK, 0);
        FilePuts(f, " : ");
        PrettyExpr(f, e->tree.right->tree.right, selfPrec, level, nest);
        PrettyMark(f, PRETTY_END, 0);
        break;
    case FORK:
        FilePuts(f, "fork ");
        PrettyExpr(f, e->tree.right, selfPrec, level, nest);
        break;
    case ISTYPE:
        /* Only a comma expression needs parentheses between the commas */
        FilePuts(f, "is_type(");
        PrettyMark(f, PRETTY_GROUP, PRETTY_CALL);
        PrettyExpr(f, e->type.left, PrettyPrecedence(COMMA) + 1, level, nest);
        FilePuts(f, ",");
        PrettyMark(f, PRETTY_BREAK, 0);
        FilePrintf(f, " %T", e->type.type);
        PrettyMark(f, PRETTY_END, 0);
        FilePuts(f, ")");
        break;
    case HASMEMBER:
        FilePuts(f, "has_member(");
        PrettyMark(f, PRETTY_GROUP, PRETTY_CALL);
        PrettyExpr(f, e->tree.left, PrettyPrecedence(COMMA) + 1, level, nest);
        FilePuts(f, ",");
        PrettyMark(f, PRETTY_BREAK, 0);
        FilePrintf(f, " %A", e->tree.right->atom.atom);
        PrettyMark(f, PRETTY_END, 0);
        FilePuts(f, ")");
        break;
    case DOLLAR:
        if (e->tree.left) {
            FilePuts(f, "$");
            PrettyExpr(f, e->tree.left, selfPrec, level, nest);
        } else
            FilePuts(f, ".");
        break;
    case EXPR:
        PrettyExpr(f, e->tree.left, selfPrec, level, nest);
        break;
    }
    if (nest && node->base.comments)
        PrettyExprComments(f, node, comment_inside_after);
    if (selfPrec < parentPrec)
        FilePuts(f, ")");
}

static void
PrettyCatch(Value f, Expr *e, int level, Bool nest, Bool *closed)
{
    CodePtr catch;

    if (!e)
        return;
    PrettyCatch(f, e->tree.left, level, nest, closed);
    /* Each catch follows the closing brace before it, as '} else' does */
    if (nest && *closed)
        FileOutput(f, ' ');
    else if (nest)
        PrettyIndent(f, 0, level);
    e = e->tree.right;
    catch = e->code.code;
    {
        /* Its parameters break as a definition's do, after '} catch name' */
        Value name = FileStringWrite();

        PrettyExpr(name, catch->base.name, 0, level, nest);
        name = FileStringString(name);
        FilePuts(f, "catch ");
        FilePutsc(f, StringChars(&name->string), name->string.length);
        PrettyBody(f, catch, level, nest, True,
                   nest && !profiling
                       ? (ptrdiff_t)level * 4 + (*closed ? 8 : 6) + PrettyTextColumns(name)
                       : -1);
    }
    if (nest)
        *closed = True;
    else
        FilePuts(f, "\n");
}

/*
 * Print a typedef statement; in nested output its type prints as
 * PrettyType lays it out, with any member comments
 */
static void
PrettyTypedef(Value f, Expr *e, int level, Bool nest)
{
    ENTER();
    Value       prefix = FileStringWrite();
    Value       names = FileStringWrite();
    Value       text = FileStringWrite();
    DeclListPtr decl;

    PrettyIndent(f, e, level);
    if (!e->decl.type) {
        PrettyDecl(f, e, level, nest);
        FilePuts(f, ";\n");
        EXIT();
        return;
    }
    FilePrintf(prefix, "%ptypedef ", e->decl.publish);
    prefix = FileStringString(prefix);
    for (decl = e->decl.decl; decl; decl = decl->next)
        FilePrintf(names, "%s%A", decl == e->decl.decl ? " " : ", ", decl->name);
    names = FileStringString(names);
    FilePutsc(text, StringChars(&prefix->string), prefix->string.length);
    PrettyType(text, e->decl.type, level, nest, (ptrdiff_t)level * 4 + PrettyTextColumns(prefix),
               PrettyTextColumns(names) + 1);
    PrettyTypeText(f, FileStringString(text), nest ? e->base.comments : 0);
    FilePutsc(f, StringChars(&names->string), names->string.length);
    FilePuts(f, ";\n");
    EXIT();
}

/*
 * Print a statement's body after its header, as clang-format does: a
 * block opens on the header's line, or on a line of its own at level
 * when the header wrapped, and closes at level, leaving the line after
 * its '}' open; any other statement goes on the next line, one level
 * in. Return whether the body was a block
 */
static Bool
PrettyBraceBody(Value f, Expr *body, int level, Bool nest, Bool wrapped)
{
    if (body->base.tag == OC) {
        if (wrapped) {
            FilePuts(f, "\n");
            PrettyIndent(f, 0, level);
            FilePuts(f, "{\n");
        } else
            FilePuts(f, " {\n");
        PrettyBlock(f, body, level + 1, nest, !wrapped);
        PrettyIndent(f, 0, level);
        FilePuts(f, "}");
        return True;
    }
    FilePuts(f, "\n");
    PrettyStatement(f, body, level + 1, level, nest);
    return False;
}

/*
 * Print the comments trailing a case's label or its block's '}' after
 * it, on its line when they fit, returning the statements after them
 */
static Expr *
PrettyCaseTrailing(Value f, Expr *statements, int level, Bool nest)
{
    ENTER();
    Expr *first;

    for (; (first = statements->tree.left) && first->base.tag == COMMENT_CONST
         && first->comment.comment->place == comment_trailing;
         statements = statements->tree.right) {
        Value one = FileStringWrite();

        if (!nest)
            continue;
        if (PrettyCommentOneLine(one, first->comment.comment->text, PRETTY_WIDTH)) {
            one = FileStringString(one);
            FileOutput(f, ' ');
            FilePutsc(f, StringChars(&one->string), one->string.length);
        } else {
            FileOutput(f, '\n');
            PrettyComment(f, first->comment.comment->text, level + 1);
        }
    }
    EXIT();
    return statements;
}

/*
 * Print the statements of a case after its label, and the comments
 * written after the label. A block starting them opens on the label's
 * line and closes at the label's level, with a break right after it on
 * the same line, as clang-format does: 'case 2: {' ... '} break;',
 * unless the break holds comments or has them trailing it
 */
static void
PrettyCaseBody(Value f, Expr *statements, int level, Bool nest)
{
    Expr *first, *next;

    /* Comments written after the label stay after it, on its line when they fit */
    statements = PrettyCaseTrailing(f, statements, level, nest);
    if ((first = statements->tree.left) && first->base.tag == OC) {
        FilePuts(f, " {\n");
        PrettyBlock(f, first, level + 1, nest, True);
        PrettyIndent(f, 0, level);
        FilePuts(f, "}");
        statements = PrettyCaseTrailing(f, statements->tree.right, level, nest);
        first = statements->tree.left;
        next = statements->tree.right ? statements->tree.right->tree.left : 0;
        if (first && first->base.tag == BREAK && !(nest && first->base.comments)
            && !(nest && next && next->base.tag == COMMENT_CONST
                 && next->comment.comment->place == comment_trailing)) {
            FilePuts(f, " break;");
            statements = statements->tree.right;
        }
    }
    FilePuts(f, "\n");
    PrettyBlock(f, statements, level + 1, nest, False);
}

/*
 * Lay out marked code. Text goes out as it is, except that spaces wait
 * until something follows them on the line, so a break drops them
 */
#define PRETTY_PARENS 64

typedef struct _prettyWrap {
    Value     out;
    ptrdiff_t col;        /* column of the next character */
    ptrdiff_t spaces;     /* spaces waiting to go out */
    ptrdiff_t indent;     /* column the statement starts in; lines are measured from it */
    ptrdiff_t start;      /* column the current line's code starts in */
    char      quote;      /* the quote of a literal being printed, or 0 */
    ptrdiff_t literal;    /* the column the literal being printed starts in */
    ptrdiff_t longest;    /* the columns of the longest literal on the line */
    Bool      escape;     /* whether a backslash came just before, in a literal */
    Bool      drop_space; /* whether to drop the next space, as a block's '}' starts a line */
    ptrdiff_t newline;    /* the column a comment's forced line break continues at */
    Bool      fresh;      /* whether the line holds nothing yet */
    Bool      forced;     /* whether a comment's forced line break started the line */
    int       parens;     /* open parentheses */
    ptrdiff_t paren[PRETTY_PARENS];        /* the column after each */
    int       chains;                      /* operator chains being broken */
    int       chain_parens[PRETTY_PARENS]; /* the open parentheses around each */
    int       brackets;                    /* open brackets and braces */
    ptrdiff_t resorts;                     /* subscripts being laid out */
    Bool      global;                      /* whether a newline outside them ends the statement */
    ptrdiff_t threshold;                   /* groups ranking after this lay out on one line */
    ptrdiff_t past;                        /* columns lines pass the end of the line by */
    ptrdiff_t over;                        /* columns lines pass their ends by */
    ptrdiff_t lines;                       /* lines laid out */
} PrettyWrap;

/* The column continued lines align to: right of the innermost open parenthesis */
static ptrdiff_t
PrettyWrapAlign(PrettyWrap *l)
{
    if (l->parens > 0 && l->parens <= PRETTY_PARENS)
        return l->paren[l->parens - 1];
    return l->indent + 4;
}

static void
PrettyLayoutChar(PrettyWrap *l, char c)
{
    if (c == ' ') {
        if (l->drop_space)
            l->drop_space = False;
        else
            l->spaces++;
        return;
    }
    l->drop_space = False;
    l->fresh = False;
    for (; l->spaces; l->spaces--) {
        FileOutput(l->out, ' ');
        l->col++;
    }
    FileOutput(l->out, c);
    if ((c & 0xc0) != 0x80)
        l->col++;
    /* Track parentheses outside string and character literals */
    if (l->quote) {
        if (l->escape)
            l->escape = False;
        else if (c == '\\')
            l->escape = True;
        else if (c == l->quote) {
            l->quote = 0;
            if (l->col - l->literal > l->longest)
                l->longest = l->col - l->literal;
        }
    } else if (c == '"' || c == '\'') {
        l->quote = c;
        l->literal = l->col - 1;
    } else if (c == '(') {
        if (l->parens < PRETTY_PARENS)
            l->paren[l->parens] = l->col;
        l->parens++;
    } else if (c == ')' && l->parens > 0)
        l->parens--;
    else if (c == '[' || c == '{')
        l->brackets++;
    else if ((c == ']' || c == '}') && l->brackets > 0)
        l->brackets--;
}

/*
 * The columns a line ending at column col, with a literal of longest
 * columns, passes end by, apart from those the literal would pass it by
 * even starting a continued line of a statement at indent
 */
static ptrdiff_t
PrettyPastEnd(ptrdiff_t col, ptrdiff_t end, ptrdiff_t longest, ptrdiff_t indent)
{
    ptrdiff_t past = col - end, unavoidable = longest - (end - (indent + 4));

    if (unavoidable > 0)
        past -= unavoidable;
    return past > 0 ? past : 0;
}

/*
 * Count the line ending at the current column, with tail more columns
 * to follow, and the columns it passes end and the end of the line by,
 * apart from those a literal too long for any line passes them by
 */
static void
PrettyWrapLineEnd(PrettyWrap *l, ptrdiff_t end, ptrdiff_t tail)
{
    l->over += PrettyPastEnd(l->col + tail, end, l->longest, l->indent);
    l->past += PrettyPastEnd(l->col + tail, PRETTY_LINE_END, l->longest, l->indent);
    l->longest = 0;
    l->lines++;
}

static void
PrettyLayoutNewline(PrettyWrap *l, ptrdiff_t align)
{
    PrettyWrapLineEnd(l, PrettyCodeEnd(l->indent), 0);
    l->spaces = 0;
    FileOutput(l->out, '\n');
    for (l->col = 0; l->col < align; l->col++)
        FileOutput(l->out, ' ');
    l->start = align;
    l->fresh = True;
    l->forced = False;
}

/*
 * Columns marked text takes on one line, or on its first line when a
 * lambda's block, an initializer's rows, or a comment ending its line
 * break it
 */
static ptrdiff_t
PrettyFlatWidth(const char *s, ptrdiff_t len)
{
    ptrdiff_t cols = 0, i;

    for (i = 0; i < len && s[i] != '\n' && s[i] != PRETTY_NEWLINE; i++) {
        if (s[i] == PRETTY_GROUP && s[i + 1] == PRETTY_ROWS)
            break;
        if (s[i] == PRETTY_GROUP)
            i++;
        else if (s[i] != PRETTY_END && s[i] != PRETTY_BREAK && (s[i] & 0xc0) != 0x80)
            cols++;
    }
    return cols;
}

/* Whether a line may break here, as a newline outside brackets ends a global statement */
static Bool
PrettyMayBreak(PrettyWrap *l)
{
    return !(l->global && l->parens == 0 && l->brackets == 0);
}

/* The lambda or initializer numbered after the first line of a shifted group's text */
static Expr *
PrettyShiftedExpr(const char *s, ptrdiff_t len)
{
    ptrdiff_t i, n = 0;

    for (i = 0; i < len && s[i] != '\n'; i++)
        ;
    for (i++; i < len && '0' <= s[i] && s[i] <= '9'; i++)
        n = n * 10 + (s[i] - '0');
    return prettyShifted[n];
}

/*
 * A lambda, or an initializer's type, printed as from column 0 to go at
 * column, its body or members four columns in
 */
static Value
PrettyShiftedText(Expr *e, ptrdiff_t column)
{
    ENTER();
    Value     text = FileStringWrite();
    Bool      was = prettyMarking;
    ptrdiff_t shift = prettyShift;

    prettyMarking = False;
    prettyShift = shift + column;
    if (e->base.tag == FUNC)
        PrettyCode(text, e->code.code, 0, class_undef, publish_private, 0, True);
    else
        PrettyNewTypeText(text, e, 0, True);
    prettyShift = shift;
    prettyMarking = was;
    RETURN(FileStringString(text));
}

/*
 * How well a lambda or an initializer's type fits at column: the lines
 * it takes, and how many of them pass the end of the line
 */
static void
PrettyShiftedFit(Expr *e, ptrdiff_t column, ptrdiff_t *lines, ptrdiff_t *over)
{
    ENTER();
    Value       text = PrettyShiftedText(e, column);
    const char *s = StringChars(&text->string);
    ptrdiff_t   i, cols = column;

    *lines = 1;
    *over = 0;
    for (i = 0; i <= text->string.length; i++) {
        if (i == text->string.length || s[i] == '\n') {
            if (cols > PRETTY_LINE_END)
                ++*over;
            if (i < text->string.length)
                ++*lines;
            cols = column;
        } else if ((s[i] & 0xc0) != 0x80)
            cols++;
    }
    EXIT();
}

/*
 * Whether a lambda or an initializer's type fits better on a line of
 * its own, four columns in from the current line, than at column, its
 * body or members having more room there: fewer of its lines pass the
 * end of the line, or as many do and it takes fewer lines
 */
static Bool
PrettyShiftedMoves(PrettyWrap *l, Expr *e, ptrdiff_t column)
{
    ptrdiff_t lines, over, moved_lines, moved_over;

    if (!PrettyMayBreak(l) || l->start + 4 >= column)
        return False;
    PrettyShiftedFit(e, column, &lines, &over);
    PrettyShiftedFit(e, l->start + 4, &moved_lines, &moved_over);
    return moved_over < over || (moved_over == over && moved_lines < lines);
}

/*
 * Lay out a shifted group, printing its lambda or initializer's type
 * from the current column: each line after the first starts that many
 * columns further in, a blank line staying blank. One taking more lines
 * there than on a line of its own, four columns in from the line it
 * was on, goes there
 */
static void
PrettyLayoutShifted(PrettyWrap *l, const char *s, ptrdiff_t len)
{
    ENTER();
    Expr       *e = PrettyShiftedExpr(s, len);
    ptrdiff_t   column = l->col + l->spaces, i;
    Value       text;
    const char *t;

    if (PrettyShiftedMoves(l, e, column)) {
        PrettyLayoutNewline(l, l->start + 4);
        column = l->start;
    }
    text = PrettyShiftedText(e, column);
    t = StringChars(&text->string);
    for (i = 0; i < text->string.length; i++) {
        if (t[i] == '\n') {
            PrettyWrapLineEnd(l, PRETTY_LINE_END, 0);
            FileOutput(l->out, '\n');
            l->col = 0;
            l->spaces = column;
            l->start = column;
        } else
            PrettyLayoutChar(l, t[i]);
    }
    EXIT();
}

/* The index of the PRETTY_END closing the group starting at s[i] */
static ptrdiff_t
PrettyGroupEnd(const char *s, ptrdiff_t i, ptrdiff_t len)
{
    ptrdiff_t depth = 0;

    for (; i < len; i++) {
        if (s[i] == PRETTY_GROUP) {
            depth++;
            i++;
        } else if (s[i] == PRETTY_END && --depth == 0)
            return i;
    }
    return len;
}

/* The lambda or initializer of the first shifted group at the top of marked text */
static Expr *
PrettyItemShifted(const char *s, ptrdiff_t len)
{
    ptrdiff_t i, end;

    for (i = 0; i < len; i++)
        if (s[i] == PRETTY_GROUP) {
            end = PrettyGroupEnd(s, i, len);
            if (s[i + 1] == PRETTY_SHIFTED)
                return PrettyShiftedExpr(s + i + 2, end - i - 2);
            i = end;
        }
    return 0;
}

/* The last column code starting at column indent may end in */
static ptrdiff_t
PrettyCodeEnd(ptrdiff_t indent)
{
    ptrdiff_t end = indent + PRETTY_CODE_WIDTH;

    if (end > PRETTY_LINE_END)
        end = PRETTY_LINE_END;
    return end;
}

static void PrettyLayoutSequence(PrettyWrap *l, const char *s, ptrdiff_t len, ptrdiff_t tail);

/*
 * Whether layout a fits better than layout b: its lines pass the end
 * of the line by fewer columns, or as many and their ends by fewer, or
 * as many and it takes fewer lines
 */
static Bool
PrettyWrapBetter(PrettyWrap *a, PrettyWrap *b)
{
    if (a->past != b->past)
        return a->past < b->past;
    if (a->over != b->over)
        return a->over < b->over;
    return a->lines < b->lines;
}

/*
 * Lay out a statement's assignment, which doesn't fit on its line: its
 * value after the operator, or on the next line, four columns in, when
 * that fits better, counting the line the operator ends
 */
static void
PrettyLayoutAssign(PrettyWrap *l, const char *s, ptrdiff_t len, ptrdiff_t tail)
{
    ENTER();
    ptrdiff_t  i;
    PrettyWrap after, moved, a, m;
    Value      out = l->out, text;

    for (i = 0; i < len && s[i] != PRETTY_BREAK; i++)
        if (s[i] == PRETTY_GROUP)
            i = PrettyGroupEnd(s, i, len);
    PrettyLayoutSequence(l, s, i, 0);
    if (i == len || !PrettyMayBreak(l) || l->start + 4 >= l->col + l->spaces) {
        if (i < len)
            PrettyLayoutSequence(l, s + i + 1, len - i - 1, tail);
        EXIT();
        return;
    }
    after = *l;
    after.out = FileStringWrite();
    PrettyLayoutSequence(&after, s + i + 1, len - i - 1, tail);
    moved = *l;
    moved.out = FileStringWrite();
    PrettyLayoutNewline(&moved, l->start + 4);
    PrettyLayoutSequence(&moved, s + i + 1, len - i - 1, tail);
    /* Compare them with their last lines ended */
    a = after;
    m = moved;
    PrettyWrapLineEnd(&a, PrettyCodeEnd(l->indent), tail);
    PrettyWrapLineEnd(&m, PrettyCodeEnd(l->indent), tail);
    if (PrettyWrapBetter(&m, &a))
        after = moved;
    text = FileStringString(after.out);
    FilePutsc(out, StringChars(&text->string), text->string.length);
    *l = after;
    l->out = out;
    EXIT();
}

/*
 * Whether the group starting at s[i] is an initializer, or a cast whose
 * value is one
 */
static Bool
PrettyHoldsInitializer(const char *s, ptrdiff_t i, ptrdiff_t len)
{
    ptrdiff_t end = PrettyGroupEnd(s, i, len), j;

    if (s[i + 1] == PRETTY_INITIALIZE || s[i + 1] == PRETTY_ROWS)
        return True;
    if (s[i + 1] != PRETTY_CAST)
        return False;
    for (j = i + 2; j < end; j++)
        if (s[j] == PRETTY_GROUP) {
            if (PrettyHoldsInitializer(s, j, end))
                return True;
            j = PrettyGroupEnd(s, j, end);
        }
    return False;
}

/* Whether marked text holds a comment's forced line break outside its groups */
static Bool
PrettyHoldsNewline(const char *s, ptrdiff_t len)
{
    ptrdiff_t i;

    for (i = 0; i < len; i++) {
        if (s[i] == PRETTY_GROUP)
            i = PrettyGroupEnd(s, i, len);
        else if (s[i] == PRETTY_NEWLINE)
            return True;
    }
    return False;
}

/* Whether marked text holds an initializer's rows */
static Bool
PrettyHoldsRows(const char *s, ptrdiff_t len)
{
    ptrdiff_t i;

    for (i = 0; i < len; i++)
        if (s[i] == PRETTY_GROUP) {
            if (s[i + 1] == PRETTY_ROWS)
                return True;
            i++;
        }
    return False;
}

static void PrettyLayoutGroup(PrettyWrap *l, const char *s, ptrdiff_t len, char kind,
                              ptrdiff_t tail);

/* Lay out marked text, with tail more columns to follow it on its last line */
static void
PrettyLayoutSequence(PrettyWrap *l, const char *s, ptrdiff_t len, ptrdiff_t tail)
{
    ptrdiff_t i, end;

    for (i = 0; i < len; i++) {
        if (s[i] == PRETTY_GROUP) {
            end = PrettyGroupEnd(s, i, len);
            PrettyLayoutGroup(l, s + i + 2, end - i - 2, s[i + 1],
                              PrettyFlatWidth(s + end + 1, len - end - 1) + tail);
            i = end;
        } else if (s[i] == PRETTY_NEWLINE) {
            /*
             * A line holding nothing yet needs no break, and a newline
             * outside brackets would end a global statement
             */
            if (l->fresh)
                ;
            else if (PrettyMayBreak(l)) {
                PrettyLayoutNewline(l, l->newline);
                l->drop_space = True;
                l->forced = True;
            } else {
                /* A space, but just one */
                PrettyLayoutChar(l, ' ');
                l->drop_space = True;
            }
        } else if (s[i] != PRETTY_END && s[i] != PRETTY_BREAK)
            PrettyLayoutChar(l, s[i]);
    }
}

/*
 * Lay out the comments starting an item that end their lines, which
 * stay with the code before them, so the item's line starts after
 * them; return the bytes of the item they take
 */
static ptrdiff_t
PrettyItemNotes(PrettyWrap *l, const char *s, ptrdiff_t len)
{
    ptrdiff_t i = 0, used = 0, end;

    for (;;) {
        if (i < len && s[i] == ' ')
            i++;
        if (i < len && s[i] == PRETTY_NEWLINE)
            i++;
        if (i + 1 >= len || s[i] != PRETTY_GROUP || s[i + 1] != PRETTY_NOTE)
            break;
        end = PrettyGroupEnd(s, i, len);
        if (end + 1 >= len || s[end + 1] != PRETTY_NEWLINE)
            break;
        i = used = end + 2;
    }
    if (used)
        PrettyLayoutSequence(l, s, used, 0);
    return used;
}

/*
 * The lines filling items, starting at column col, takes when the first
 * line may end by column first and the others by column later; a break
 * before an item starts the next line at column align, dropping the
 * item's leading space, as a forced one always does
 */
static ptrdiff_t
PrettyFillCount(const ptrdiff_t *widths, const Bool *lead, const Bool *forced, ptrdiff_t n,
                ptrdiff_t col, ptrdiff_t align, ptrdiff_t first, ptrdiff_t later, ptrdiff_t tail)
{
    ptrdiff_t lines = 1, k, last = first;

    for (k = 0; k < n; k++) {
        ptrdiff_t end = col + widths[k] + (k == n - 1 ? tail : 0);

        if (k > 0 && (end > last || forced[k])) {
            lines++;
            col = align + widths[k] - (ptrdiff_t)lead[k];
            last = later;
        } else
            col += widths[k];
    }
    return lines;
}

/*
 * Split marked text at the breaks outside its groups, storing where
 * each piece starts and how long it is, when there are no more than
 * max; returns how many there are
 */
static ptrdiff_t
PrettySplit(const char *s, ptrdiff_t len, ptrdiff_t *starts, ptrdiff_t *lens, ptrdiff_t max)
{
    ptrdiff_t i, start = 0, n = 0;

    for (i = 0; i <= len; i++) {
        if (i < len && s[i] == PRETTY_GROUP)
            i = PrettyGroupEnd(s, i, len);
        else if (i == len || s[i] == PRETTY_BREAK) {
            if (n < max) {
                starts[n] = start;
                lens[n] = i - start;
            }
            n++;
            start = i + 1;
        }
    }
    return n;
}

/* Whether marked text is a number, perhaps negative: a digit first and no spaces */
static Bool
PrettyIsNumber(const char *s, ptrdiff_t len)
{
    ptrdiff_t i = 0;

    if (i < len && s[i] == '-')
        i++;
    if (i == len || s[i] < '0' || '9' < s[i])
        return False;
    for (; i < len; i++)
        if (s[i] == ' ' || s[i] == PRETTY_GROUP)
            return False;
    return True;
}

/*
 * How many items to a line rows line up in columns with, or 0 when
 * they don't: the rows, row r holding counts[r] items from first[r] in
 * widths, hold as many items as each other, the last perhaps fewer; the
 * columns are as wide as their widest item; and every line, starting at
 * column start with lead more columns before its items and ending with
 * ',' or, ending a row, tail columns, ends by column end. A row too long
 * splits evenly over lines of fewer items, as many as divide its items,
 * but not one to a line, and at least two lines are full. Stores each
 * column's width in columns and whether it lines up on the right, as
 * when it holds only numbers, in right
 */
static ptrdiff_t
PrettyColumnsFit(const ptrdiff_t *first, const ptrdiff_t *counts, ptrdiff_t nrows,
                 const ptrdiff_t *widths, const Bool *numbers, ptrdiff_t start, ptrdiff_t lead,
                 ptrdiff_t tail, ptrdiff_t end, ptrdiff_t *columns, Bool *right)
{
    ptrdiff_t ncells = 0, per, r, c, k, n, width;

    for (r = 0; r < nrows; r++)
        if (counts[r] > ncells)
            ncells = counts[r];
    for (r = 0; r + 1 < nrows; r++)
        if (counts[r] != ncells)
            return 0;
    for (per = ncells; per > 1 || (per == 1 && ncells == 1); per--) {
        if (ncells % per != 0)
            continue;
        for (c = 0; c < per; c++) {
            columns[c] = 0;
            right[c] = True;
        }
        for (r = 0; r < nrows; r++)
            for (c = 0; c < counts[r]; c++) {
                k = first[r] + c;
                if (widths[k] > columns[c % per])
                    columns[c % per] = widths[k];
                if (!numbers[k])
                    right[c % per] = False;
            }
        for (r = 0; r < nrows; r++) {
            for (c = 0, width = 0; c < counts[r]; c++) {
                k = first[r] + c;
                if (c % per == 0)
                    width = start + lead;
                if (c % per < per - 1 && c < counts[r] - 1)
                    width += columns[c % per] + 2;
                else {
                    width += (right[c % per] ? columns[c % per] : widths[k])
                        + (c == counts[r] - 1 ? tail : 1);
                    if (width > end)
                        break;
                }
            }
            if (c < counts[r])
                break;
        }
        if (r == nrows)
            break;
    }
    if (per == 0 || (per == 1 && ncells > 1))
        return 0;
    /* At least two full lines make a table */
    for (r = 0, n = 0; r < nrows; r++)
        n += counts[r] / per;
    return n < 2 ? 0 : per;
}

/* Whether marked text is just comments: notes and the spaces between them */
static Bool
PrettyJustNotes(const char *s, ptrdiff_t len)
{
    ptrdiff_t i;
    Bool      any = False;

    for (i = 0; i < len; i++) {
        if (s[i] == ' ')
            continue;
        if (s[i] != PRETTY_GROUP || s[i + 1] != PRETTY_NOTE)
            return False;
        i = PrettyGroupEnd(s, i, len);
        any = True;
    }
    return any;
}

/* Where the notes ending marked text start, at the space before them, or len when none do */
static ptrdiff_t
PrettyTrailingNotes(const char *s, ptrdiff_t len)
{
    ptrdiff_t i, run = -1;

    for (i = 0; i < len; i++) {
        if (s[i] == ' ' && i + 2 < len && s[i + 1] == PRETTY_GROUP && s[i + 2] == PRETTY_NOTE) {
            if (run < 0)
                run = i;
            i = PrettyGroupEnd(s, i + 1, len);
            continue;
        }
        if (s[i] == PRETTY_GROUP)
            i = PrettyGroupEnd(s, i, len);
        run = -1;
    }
    return run < 0 ? len : run;
}

/*
 * Lay out an initializer's rows in columns, when they line up
 * (PrettyColumnsFit) and none holds a lambda's block or rows: their
 * items, or, when every row holds just an initializer, its items,
 * numbers lined up on the right and anything else on the left. Rows of
 * items that don't line up as written may when filled to the end of
 * the code, as they would print without lining up, and then line up
 * that way, as they would print again. Comments on lines of their own
 * stay there, and those ending rows line up past the widest line,
 * their widths leaving the columns alone; one ending a line inside an
 * item leaves the rows as they are. Returns False, laying out nothing,
 * when the rows don't line up
 */
static Bool
PrettyLayoutColumns(PrettyWrap *l, const char *s, ptrdiff_t len, ptrdiff_t outer)
{
    ENTER();
    ptrdiff_t   nsource = PrettySplit(s, len, 0, 0, 0), nrows = 0, ncells = 0, n, r, c, k, i, per;
    ptrdiff_t   end = PrettyCodeEnd(l->indent), note_column, line_end, t;
    ptrdiff_t  *source_starts, *source_lens, *note_starts, *note_lens, *table_row;
    ptrdiff_t  *row_starts, *row_lens, *cell_starts, *cell_lens, *counts, *first, *widths;
    ptrdiff_t  *columns, *inner_starts, *inner_lens, *line_row;
    const char *row;
    Bool       *numbers, *right, *note_row, *last_line;
    Bool        inits, notes = False;

    /* A comment's forced line break leaves no line to measure */
    if (memchr(s, PRETTY_NEWLINE, (size_t)len)) {
        EXIT();
        return False;
    }
    source_starts = AllocateTemp((size_t)nsource * sizeof(ptrdiff_t));
    source_lens = AllocateTemp((size_t)nsource * sizeof(ptrdiff_t));
    note_starts = AllocateTemp((size_t)nsource * sizeof(ptrdiff_t));
    note_lens = AllocateTemp((size_t)nsource * sizeof(ptrdiff_t));
    note_row = AllocateTemp((size_t)nsource * sizeof(Bool));
    table_row = AllocateTemp((size_t)nsource * sizeof(ptrdiff_t));
    PrettySplit(s, len, source_starts, source_lens, nsource);
    /*
     * Each row is a group of items, perhaps followed by comments, or a
     * group of just comments on lines of their own
     */
    for (r = 0; r < nsource; r++) {
        ptrdiff_t rlen = source_lens[r];

        row = s + source_starts[r];
        if (rlen < 2 || row[0] != PRETTY_GROUP || row[1] != PRETTY_ROW) {
            EXIT();
            return False;
        }
        rlen = PrettyGroupEnd(row, 0, rlen);
        source_starts[r] += 2;
        source_lens[r] = rlen - 2;
        row = s + source_starts[r];
        note_row[r] = PrettyJustNotes(row, source_lens[r]);
        if (note_row[r]) {
            note_starts[r] = source_starts[r];
            note_lens[r] = source_lens[r];
            table_row[r] = -1;
            continue;
        }
        n = PrettyTrailingNotes(row, source_lens[r]);
        note_starts[r] = source_starts[r] + n;
        note_lens[r] = source_lens[r] - n;
        if (note_lens[r])
            notes = True;
        source_lens[r] = n;
        table_row[r] = nrows++;
    }
    if (nrows == 0) {
        EXIT();
        return False;
    }
    row_starts = AllocateTemp((size_t)nrows * sizeof(ptrdiff_t));
    row_lens = AllocateTemp((size_t)nrows * sizeof(ptrdiff_t));
    inner_starts = AllocateTemp((size_t)nrows * sizeof(ptrdiff_t));
    inner_lens = AllocateTemp((size_t)nrows * sizeof(ptrdiff_t));
    for (r = 0; r < nsource; r++)
        if (table_row[r] >= 0) {
            row_starts[table_row[r]] = source_starts[r];
            row_lens[table_row[r]] = source_lens[r];
        }
    /* Find the rows' items, or, when every row holds just an initializer, the initializers' */
    inits = True;
    for (r = 0; r < nrows; r++) {
        ptrdiff_t   one_start, one_len, gend;
        const char *u;

        row = s + row_starts[r];
        n = PrettySplit(row, row_lens[r], &one_start, &one_len, 1);
        /* '{ ', the initializer's group, then ' },' */
        u = row + one_start;
        if (n == 1 && one_len > 4 && u[0] == '{' && u[1] == ' ' && u[2] == PRETTY_GROUP
            && u[3] == PRETTY_INITIALIZE && (gend = PrettyGroupEnd(u, 2, one_len)) + 4 == one_len
            && memcmp(u + gend + 1, " },", 3) == 0) {
            inner_starts[r] = row_starts[r] + one_start + 4;
            inner_lens[r] = gend - 4;
        } else
            inits = False;
    }
    /* The items in order, row by row */
    counts = AllocateTemp((size_t)nrows * sizeof(ptrdiff_t));
    first = AllocateTemp((size_t)nrows * sizeof(ptrdiff_t));
    for (r = 0; r < nrows; r++) {
        if (inits) {
            row_starts[r] = inner_starts[r];
            row_lens[r] = inner_lens[r];
        }
        counts[r] = PrettySplit(s + row_starts[r], row_lens[r], 0, 0, 0);
        first[r] = ncells;
        ncells += counts[r];
    }
    cell_starts = AllocateTemp((size_t)ncells * sizeof(ptrdiff_t));
    cell_lens = AllocateTemp((size_t)ncells * sizeof(ptrdiff_t));
    widths = AllocateTemp((size_t)ncells * sizeof(ptrdiff_t));
    numbers = AllocateTemp((size_t)ncells * sizeof(Bool));
    columns = AllocateTemp((size_t)ncells * sizeof(ptrdiff_t));
    right = AllocateTemp((size_t)ncells * sizeof(Bool));
    /* The items without the spaces before them or the ', ' or ',' after them */
    for (r = 0; r < nrows; r++) {
        row = s + row_starts[r];
        PrettySplit(row, row_lens[r], cell_starts + first[r], cell_lens + first[r], counts[r]);
        for (k = first[r]; k < first[r] + counts[r]; k++) {
            cell_starts[k] += row_starts[r];
            while (cell_lens[k] > 0 && s[cell_starts[k]] == ' ') {
                cell_starts[k]++;
                cell_lens[k]--;
            }
            while (cell_lens[k] > 0
                   && (s[cell_starts[k] + cell_lens[k] - 1] == ' '
                       || s[cell_starts[k] + cell_lens[k] - 1] == ','))
                cell_lens[k]--;
            for (i = 0; i < cell_lens[k]; i++)
                if (s[cell_starts[k] + i] == '\n'
                    || (s[cell_starts[k] + i] == PRETTY_GROUP
                        && (s[cell_starts[k] + i + 1] == PRETTY_SHIFTED
                            || s[cell_starts[k] + i + 1] == PRETTY_ROWS))) {
                    EXIT();
                    return False;
                }
            widths[k] = PrettyFlatWidth(s + cell_starts[k], cell_lens[k]);
            numbers[k] = PrettyIsNumber(s + cell_starts[k], cell_lens[k]);
        }
    }
    /* Each line of the table, its row in the source, and whether it ends the row */
    line_row = AllocateTemp((size_t)ncells * sizeof(ptrdiff_t));
    last_line = AllocateTemp((size_t)ncells * sizeof(Bool));
    for (r = 0; r < nrows; r++) {
        line_row[r] = r;
        last_line[r] = True;
    }
    per = PrettyColumnsFit(first, counts, nrows, widths, numbers, outer + 4, inits ? 2 : 0,
                           inits ? 3 : 1, end, columns, right);
    if (!per && !inits) {
        /*
         * The lines filling each row makes, each item followed by ','
         * and those after the first on a line led by a space; an item
         * too wide for a line of its own would break, and nothing lines
         * up
         */
        ptrdiff_t *line_first = AllocateTemp((size_t)ncells * sizeof(ptrdiff_t));
        ptrdiff_t *line_counts = AllocateTemp((size_t)ncells * sizeof(ptrdiff_t));
        ptrdiff_t  nlines = 0, col;

        for (r = 0; r < nrows; r++) {
            col = outer + 4;
            for (k = first[r]; k < first[r] + counts[r]; k++) {
                if (outer + 4 + widths[k] + 1 > end) {
                    EXIT();
                    return False;
                }
                if (k == first[r] || col + 1 + widths[k] + 1 > end) {
                    line_first[nlines] = k;
                    line_counts[nlines] = 0;
                    line_row[nlines] = r;
                    last_line[nlines] = False;
                    nlines++;
                    col = outer + 4 + widths[k] + 1;
                } else
                    col += 1 + widths[k] + 1;
                line_counts[nlines - 1]++;
            }
            last_line[nlines - 1] = True;
        }
        per = PrettyColumnsFit(line_first, line_counts, nlines, widths, numbers, outer + 4, 0, 1,
                               end, columns, right);
        first = line_first;
        counts = line_counts;
        nrows = nlines;
    }
    if (!per) {
        EXIT();
        return False;
    }
    /*
     * The comments ending rows line up one space past the widest line,
     * as long as they fit there before the end of the line
     */
    note_column = 0;
    if (notes) {
        for (r = 0; r < nrows; r++) {
            line_end = 0;
            for (c = 0; c < counts[r]; c++) {
                k = first[r] + c;
                if (c % per == 0)
                    line_end = outer + 4 + (inits ? 2 : 0);
                if (c % per < per - 1 && c < counts[r] - 1)
                    line_end += columns[c % per] + 2;
                else {
                    line_end += (right[c % per] ? columns[c % per] : widths[k])
                        + (c == counts[r] - 1 && inits ? 3 : 1);
                    if (line_end + 1 > note_column)
                        note_column = line_end + 1;
                }
            }
        }
        for (t = 0; t < nsource; t++)
            if (!note_row[t] && note_lens[t]
                && note_column + PrettyFlatWidth(s + note_starts[t] + 1, note_lens[t] - 1)
                    > PRETTY_LINE_END) {
                EXIT();
                return False;
            }
    }
    t = 0;
    for (r = 0; r < nrows; r++) {
        /* The comments on lines of their own before the row */
        if (r == 0 || last_line[r - 1])
            for (; t < nsource && (note_row[t] || table_row[t] < line_row[r]); t++)
                if (note_row[t]) {
                    PrettyLayoutNewline(l, outer + 4);
                    PrettyLayoutSequence(l, s + note_starts[t], note_lens[t], 0);
                }
        for (c = 0; c < counts[r]; c++) {
            k = first[r] + c;
            if (c % per == 0) {
                PrettyLayoutNewline(l, outer + 4 + (inits && c > 0 ? 2 : 0));
                if (inits && c == 0) {
                    PrettyLayoutChar(l, '{');
                    PrettyLayoutChar(l, ' ');
                }
            }
            if (right[c % per])
                l->spaces += columns[c % per] - widths[k];
            PrettyLayoutSequence(l, s + cell_starts[k], cell_lens[k], 0);
            if (inits && c == counts[r] - 1) {
                PrettyLayoutChar(l, ' ');
                PrettyLayoutChar(l, '}');
            }
            PrettyLayoutChar(l, ',');
            if (!right[c % per])
                l->spaces += columns[c % per] - widths[k];
            l->spaces++;
        }
        /* The comments ending the row */
        if (last_line[r]) {
            for (; t < nsource && table_row[t] != line_row[r]; t++)
                ;
            if (t < nsource && note_lens[t]) {
                l->spaces = note_column - l->col;
                PrettyLayoutSequence(l, s + note_starts[t] + 1, note_lens[t] - 1, 0);
            }
            t++;
        }
    }
    /* The comments on lines of their own after the last row */
    for (; t < nsource; t++)
        if (note_row[t]) {
            PrettyLayoutNewline(l, outer + 4);
            PrettyLayoutSequence(l, s + note_starts[t], note_lens[t], 0);
        }
    EXIT();
    return True;
}

/*
 * Lay out a group, on one line when it fits there and otherwise filling
 * lines with its items, broken where clang-format breaks them: call
 * arguments and initializers after their commas, aligned after the
 * opening '(' or '{ '; operators before each operator, right of the
 * innermost open parenthesis, or four columns in from the statement
 * outside any; ?: before the ':', under the '?'. The fill is
 * balanced: as many lines as filling to the limit takes, but filled to
 * the narrowest width that takes no more
 */
static void
PrettyLayoutGroupIn(PrettyWrap *l, const char *s, ptrdiff_t len, char kind, ptrdiff_t tail)
{
    ENTER();
    ptrdiff_t  first = PrettyCodeEnd(l->indent), later = first;
    ptrdiff_t  n = 1, i, k, start, align, lo, hi, count, col, last;
    Bool       chain = False, nested = False, holds = False, lambdas = False, opener = False;
    Bool       rows, newline;
    ptrdiff_t *starts, *lens, *widths;
    Bool      *lead, *lambda, *forced;

    if (kind == PRETTY_SHIFTED) {
        PrettyLayoutShifted(l, s, len);
        EXIT();
        return;
    }
    /*
     * An initializer's rows always break: each row starts a line four
     * columns in from the line holding the '{', lined up in columns when
     * they can be, and the '}' starts a line of its own at that line's
     * column
     */
    if (kind == PRETTY_ROWS) {
        ptrdiff_t outer = l->start;

        l->newline = outer + 4;
        if (!PrettyLayoutColumns(l, s, len, outer))
            for (i = 0, start = 0; i <= len; i++) {
                if (i < len && s[i] == PRETTY_GROUP)
                    i = PrettyGroupEnd(s, i, len);
                else if (i == len || s[i] == PRETTY_BREAK) {
                    PrettyLayoutNewline(l, outer + 4);
                    PrettyLayoutSequence(l, s + start, i - start, 0);
                    start = i + 1;
                }
            }
        PrettyLayoutNewline(l, outer);
        l->drop_space = True;
        EXIT();
        return;
    }
    /*
     * An initializer or row holding rows breaks between its items, as
     * its width on one line ends at the rows; an initializer holding
     * them opens as a block
     */
    rows = (kind == PRETTY_INITIALIZE || kind == PRETTY_ROW) && PrettyHoldsRows(s, len);
    /* A comment ending its line breaks the group holding it */
    newline = PrettyHoldsNewline(s, len);
    if (kind == PRETTY_ASSIGN
        && PrettyGroupPriority(kind, l->parens + l->brackets, l->resorts) <= l->threshold
        && l->col + l->spaces + PrettyFlatWidth(s, len) + tail > first) {
        PrettyLayoutAssign(l, s, len, tail);
        EXIT();
        return;
    }
    /* Arguments or initializers holding a lambda's block always break before it */
    if (kind == PRETTY_CALL || kind == PRETTY_SUBSCRIPT || kind == PRETTY_INITIALIZE)
        for (i = 0; i < len; i++)
            if (s[i] == PRETTY_GROUP) {
                if (s[i + 1] == PRETTY_SHIFTED)
                    lambdas = True;
                i = PrettyGroupEnd(s, i, len);
            }
    /*
     * A group ranking after the threshold stays on one line, as does
     * one that fits there; a global statement may break only inside
     * parentheses, brackets, or braces
     */
    if ((!lambdas && !rows && !newline
         && (PrettyGroupPriority(kind, (ptrdiff_t)(l->parens + l->brackets), l->resorts)
                 > l->threshold
             || l->col + l->spaces + PrettyFlatWidth(s, len) + tail <= first))
        || !PrettyMayBreak(l)) {
        PrettyLayoutSequence(l, s, len, tail);
        EXIT();
        return;
    }
    for (i = 0; i < len; i++) {
        if (s[i] == PRETTY_GROUP)
            i = PrettyGroupEnd(s, i, len);
        else if (s[i] == PRETTY_BREAK)
            n++;
    }
    starts = AllocateTemp((size_t)n * sizeof(ptrdiff_t));
    lens = AllocateTemp((size_t)n * sizeof(ptrdiff_t));
    widths = AllocateTemp((size_t)n * sizeof(ptrdiff_t));
    lead = AllocateTemp((size_t)n * sizeof(Bool));
    lambda = AllocateTemp((size_t)n * sizeof(Bool));
    forced = AllocateTemp((size_t)n * sizeof(Bool));
    for (k = 0; k < n; k++)
        lambda[k] = False;
    for (i = 0, k = 0, start = 0; i <= len; i++) {
        if (i < len && s[i] == PRETTY_GROUP) {
            if (PrettyHoldsInitializer(s, i, len))
                holds = True;
            if (s[i + 1] == PRETTY_SHIFTED)
                lambda[k] = True;
            i = PrettyGroupEnd(s, i, len);
        } else if (i == len || s[i] == PRETTY_BREAK) {
            ptrdiff_t skip = start;

            starts[k] = start;
            lens[k] = i - start;
            /* A forced break starts an item's line, after one in the item before */
            while (skip < i && s[skip] == ' ')
                skip++;
            forced[k] = k > 0
                && ((skip < i && s[skip] == PRETTY_NEWLINE)
                    || PrettyHoldsNewline(s + starts[k - 1], lens[k - 1]));
            if (skip < i && s[skip] == PRETTY_NEWLINE)
                start = skip + 1;
            widths[k] = PrettyFlatWidth(s + start, i - start);
            lead[k] = i > start && s[start] == ' ';
            /*
             * An item holding an initializer too wide to line up after
             * the '{ ', or rows
             */
            if (holds
                && (l->col + l->spaces + widths[k] - (ptrdiff_t)lead[k] > first
                    || PrettyHoldsRows(s + start, i - start)))
                nested = True;
            holds = False;
            k++;
            start = i + 1;
        }
    }
    col = l->col + l->spaces;
    /*
     * Items holding a lambda's block start lines of their own, lined up
     * after the opening '(' or '{ ', as other items do when they don't fit
     */
    /*
     * A for's or twixt's clauses, or a comma expression's operands, that
     * don't fit on one line each start a line, as written by hand
     */
    if (kind == PRETTY_CLAUSES || kind == PRETTY_COMMAS) {
        l->newline = col;
        for (k = 0; k < n; k++) {
            ptrdiff_t used = PrettyItemNotes(l, s + starts[k], lens[k]);

            if (!used && k > 0) {
                PrettyLayoutNewline(l, col);
                used = lead[k];
            }
            PrettyLayoutSequence(l, s + starts[k] + used, lens[k] - used, k == n - 1 ? tail : 0);
        }
        EXIT();
        return;
    }
    if (lambdas) {
        /*
         * When a lambda takes more lines there than on a line of its own
         * four columns in, the items start on the line after the opener,
         * four columns in, instead
         */
        align = col;
        for (k = 0; k < n; k++)
            if (lambda[k] && PrettyShiftedMoves(l, PrettyItemShifted(s + starts[k], lens[k]), col))
                align = l->start + 4;
        l->newline = align;
        if (align != col) {
            PrettyLayoutNewline(l, align);
            if (kind == PRETTY_CALL && l->parens > 0 && l->parens <= PRETTY_PARENS)
                l->paren[l->parens - 1] = align;
        }
        for (k = 0; k < n; k++) {
            ptrdiff_t used = PrettyItemNotes(l, s + starts[k], lens[k]);

            if (used)
                ;
            else if (k == 0 && align != col)
                used = lead[k];
            else if (k > 0 && (lambda[k] || l->col + l->spaces + widths[k] > first)) {
                if (!l->fresh)
                    PrettyLayoutNewline(l, align);
                used = lead[k];
            }
            PrettyLayoutSequence(l, s + starts[k] + used, lens[k] - used, 0);
        }
        EXIT();
        return;
    }
    /*
     * An initializer holding an initializer too wide to line up after
     * its '{ ' opens as a block: its items one per line, four columns in
     * from the line holding the '{', and its '}' starting a line of its
     * own at that line's column
     */
    if (kind == PRETTY_INITIALIZE && nested) {
        ptrdiff_t outer = l->start;

        l->newline = outer + 4;
        for (k = 0; k < n; k++) {
            ptrdiff_t used = PrettyItemNotes(l, s + starts[k], lens[k]);

            if (!l->fresh)
                PrettyLayoutNewline(l, outer + 4);
            if (!used)
                used = lead[k];
            PrettyLayoutSequence(l, s + starts[k] + used, lens[k] - used, 0);
        }
        PrettyLayoutNewline(l, outer);
        l->drop_space = True;
        EXIT();
        return;
    }
    /* Member accesses and a cast's value line up as operands do */
    switch (PrettyChainKind(kind) || kind == PRETTY_MEMBERS || kind == PRETTY_CAST
                ? PRETTY_OPERATORS
                : kind) {
    case PRETTY_OPERATORS:
        /*
         * A chain inside another, within the same parentheses, goes four
         * columns further; one starting a line after a comment's forced
         * line break lines up with its start
         */
        align = l->fresh && l->forced ? l->col : PrettyWrapAlign(l);
        for (i = 0; i < l->chains && i < PRETTY_PARENS; i++)
            if (l->chain_parens[i] == l->parens)
                align += 4;
        if (l->chains < PRETTY_PARENS)
            l->chain_parens[l->chains] = l->parens;
        l->chains++;
        chain = True;
        break;
    case PRETTY_CONDITION:
        /* 'c ? a' then ': b' under the '?', or each on its own line when that doesn't fit */
        if (n == 3 && col + widths[0] + widths[1] <= first) {
            PrettyLayoutSequence(l, s + starts[0], lens[0], 0);
            PrettyLayoutSequence(l, s + starts[1], lens[1], 0);
            PrettyLayoutNewline(l, col + widths[0] + 1);
            PrettyLayoutSequence(l, s + starts[2] + 1, lens[2] - 1, tail);
            EXIT();
            return;
        }
        align = PrettyWrapAlign(l);
        l->newline = align;
        for (k = 0; k < n; k++) {
            ptrdiff_t used = PrettyItemNotes(l, s + starts[k], lens[k]);

            if (!used && k > 0) {
                PrettyLayoutNewline(l, align);
                used = 1;
            }
            PrettyLayoutSequence(l, s + starts[k] + used, lens[k] - used, k == n - 1 ? tail : 0);
        }
        EXIT();
        return;
    default:
        align = col;
        /*
         * Break right after the opening '(' or '{', with the items four
         * columns in from the line, when fewer items pass the end of the
         * line there, or as many do and that takes no more lines, counting
         * the one the opener ends, than lining them up after it
         */
        if (kind != PRETTY_CLAUSES && kind != PRETTY_ROW) {
            ptrdiff_t over = 0, moved_over = 0;

            for (k = 0; k < n; k++) {
                if (col + widths[k] - (ptrdiff_t)lead[k] > later)
                    over++;
                if (l->start + 4 + widths[k] - (ptrdiff_t)lead[k] > later)
                    moved_over++;
            }
            opener = moved_over < over
                || (moved_over == over
                    && 1
                            + PrettyFillCount(widths, lead, forced, n, l->start + 4, l->start + 4,
                                              later, later, tail)
                        <= PrettyFillCount(widths, lead, forced, n, col, col, first, later, tail));
        }
        if (opener) {
            ptrdiff_t used;

            align = l->start + 4;
            /* Comments ending the opener's line stay there */
            l->newline = align;
            used = PrettyItemNotes(l, s + starts[0], lens[0]);
            starts[0] += used;
            lens[0] -= used;
            if (!l->fresh)
                PrettyLayoutNewline(l, align);
            if (kind == PRETTY_CALL && l->parens > 0 && l->parens <= PRETTY_PARENS)
                l->paren[l->parens - 1] = align;
            col = align;
            first = later;
        }
        break;
    }

    /*
     * Balance: the narrowest width for the group's text on each line,
     * from where it starts there, taking no more lines than filling to
     * the limits does. A row fills each line instead, as each line
     * prints back as a row of its own, laid out the same way, and so
     * does a group with a line break from the source
     */
    if (kind != PRETTY_ROW && !newline) {
        count = PrettyFillCount(widths, lead, forced, n, col, align, first, later, tail);
        lo = 1;
        hi = later - align > first - col ? later - align : first - col;
        while (lo < hi) {
            ptrdiff_t mid = lo + (hi - lo) / 2;
            ptrdiff_t f = col + mid < first ? col + mid : first;
            ptrdiff_t t = align + mid < later ? align + mid : later;

            if (PrettyFillCount(widths, lead, forced, n, col, align, f, t, tail) <= count)
                hi = mid;
            else
                lo = mid + 1;
        }
        if (col + hi < first)
            first = col + hi;
        if (align + hi < later)
            later = align + hi;
    }

    last = first;
    l->newline = align;
    for (k = 0; k < n; k++) {
        const char *item = s + starts[k];
        ptrdiff_t   ilen = lens[k];
        ptrdiff_t   itail = k == n - 1 ? tail : 0;
        ptrdiff_t   lines = l->lines, used = PrettyItemNotes(l, item, ilen);

        /* After a comment's forced line break, a space leading the item goes */
        if (used) {
            item += used;
            ilen -= used;
        } else if (k > 0 && l->fresh && lead[k]) {
            item++;
            ilen--;
        } else if (k > 0 && l->col + l->spaces + widths[k] + itail > last) {
            PrettyLayoutNewline(l, align);
            last = later;
            if (lead[k]) {
                item++;
                ilen--;
            }
        }
        PrettyLayoutSequence(l, item, ilen, itail);
        if (l->lines != lines)
            last = later;
    }
    if (chain)
        l->chains--;
    EXIT();
}

/* Lay out a group, counting it while it's a subscript */
/* Lay out a comment's text as it is: its quotes and brackets aren't code's */
static void
PrettyLayoutNote(PrettyWrap *l, const char *s, ptrdiff_t len)
{
    ptrdiff_t i;

    l->drop_space = False;
    l->fresh = False;
    for (; l->spaces; l->spaces--) {
        FileOutput(l->out, ' ');
        l->col++;
    }
    for (i = 0; i < len; i++) {
        FileOutput(l->out, s[i]);
        if ((s[i] & 0xc0) != 0x80)
            l->col++;
    }
}

static void
PrettyLayoutGroup(PrettyWrap *l, const char *s, ptrdiff_t len, char kind, ptrdiff_t tail)
{
    Bool      resort = PrettyLastResortInside(kind);

    ptrdiff_t newline = l->newline;

    if (kind == PRETTY_NOTE) {
        PrettyLayoutNote(l, s, len);
        return;
    }
    if (resort)
        l->resorts++;
    PrettyLayoutGroupIn(l, s, len, kind, tail);
    if (resort)
        l->resorts--;
    l->newline = newline;
}

/*
 * Whether marked text has an operator chain or ?: outside its brackets,
 * or, when newlines is set, a comment's forced line break, which can
 * break only where a newline would end a global statement
 */
static Bool
PrettyBreaksOutside(const char *s, ptrdiff_t len, Bool newlines)
{
    ptrdiff_t i, depth = 0;
    char      quote = 0;

    for (i = 0; i < len; i++) {
        if (quote) {
            if (s[i] == '\\')
                i++;
            else if (s[i] == quote)
                quote = 0;
        } else if (s[i] == '"' || s[i] == '\'')
            quote = s[i];
        else if (s[i] == '(' || s[i] == '[' || s[i] == '{')
            depth++;
        else if ((s[i] == ')' || s[i] == ']' || s[i] == '}') && depth > 0)
            depth--;
        else if (s[i] == PRETTY_NEWLINE) {
            if (depth == 0 && newlines)
                return True;
        } else if (s[i] == PRETTY_GROUP) {
            if (depth == 0 && !newlines
                && (PrettyChainKind(s[i + 1]) || s[i + 1] == PRETTY_CONDITION))
                return True;
            /* A comment's brackets and quotes aren't code's */
            if (s[i + 1] == PRETTY_NOTE)
                i = PrettyGroupEnd(s, i, len);
            else
                i++;
        }
    }
    return False;
}

/*
 * The priorities of the groups in marked text, as PrettyLayoutGroup
 * ranks them, depth bracket levels in, stored in priorities, sorted and
 * each once; returns how many
 */
static ptrdiff_t
PrettyPriorities(const char *s, ptrdiff_t len, ptrdiff_t depth, ptrdiff_t *priorities,
                 ptrdiff_t max)
{
    ptrdiff_t i, j, k, n = 0, open = 0, resorts = 0;
    char      quote = 0;
    /* Whether each open group is a last resort */
    Bool      resort[PRETTY_PARENS];

    for (i = 0; i < len; i++) {
        if (quote) {
            if (s[i] == '\\')
                i++;
            else if (s[i] == quote)
                quote = 0;
        } else if (s[i] == '"' || s[i] == '\'')
            quote = s[i];
        else if (s[i] == '(' || s[i] == '[' || s[i] == '{')
            depth++;
        else if ((s[i] == ')' || s[i] == ']' || s[i] == '}') && depth > 0)
            depth--;
        else if (s[i] == PRETTY_GROUP) {
            ptrdiff_t p;

            /* A lambda's text lays out on its own, and a comment's brackets aren't code's */
            if (s[i + 1] == PRETTY_SHIFTED || s[i + 1] == PRETTY_NOTE) {
                i = PrettyGroupEnd(s, i, len);
                continue;
            }
            if (PrettyLastResortInside(s[i + 1]))
                resorts++;
            if (open < PRETTY_PARENS)
                resort[open] = PrettyLastResortInside(s[i + 1]);
            open++;
            p = PrettyGroupPriority(s[i + 1], depth, resorts);
            for (k = 0; k < n && priorities[k] < p; k++)
                ;
            if ((k == n || priorities[k] != p) && n < max) {
                for (j = n; j > k; j--)
                    priorities[j] = priorities[j - 1];
                priorities[k] = p;
                n++;
            }
            i++;
        } else if (s[i] == PRETTY_END && open > 0) {
            open--;
            if (open < PRETTY_PARENS && resort[open])
                resorts--;
        }
    }
    return n;
}

/*
 * Lay out marked text into out, breaking only groups ranking at or
 * before threshold, and return the layout's state
 */
static void
PrettyWrapPass(PrettyWrap *l, Value out, const char *s, ptrdiff_t len, int level, ptrdiff_t col,
               ptrdiff_t extra, ptrdiff_t tail, Bool paren, Bool around, ptrdiff_t threshold)
{
    l->out = out;
    l->col = col;
    l->spaces = 0;
    l->indent = (ptrdiff_t)level * 4;
    l->start = l->indent + extra;
    l->quote = 0;
    l->literal = 0;
    l->longest = 0;
    l->escape = False;
    l->drop_space = False;
    l->newline = l->indent + 4;
    l->fresh = False;
    l->forced = False;
    l->parens = 0;
    l->chains = 0;
    l->brackets = 0;
    l->resorts = 0;
    l->global = level == 0;
    l->threshold = threshold;
    l->past = 0;
    l->over = 0;
    l->lines = 0;
    if (paren) {
        l->paren[0] = col + extra;
        l->parens = 1;
    }
    if (around) {
        ptrdiff_t i = 0, end;

        /* Comments leading the text that don't end their lines stay before the '(' */
        while (i + 1 < len && s[i] == PRETTY_GROUP && s[i + 1] == PRETTY_NOTE) {
            end = PrettyGroupEnd(s, i, len);
            while (end + 1 < len && s[end + 1] == ' ')
                end++;
            if (end + 1 < len && s[end + 1] == PRETTY_NEWLINE)
                break;
            PrettyLayoutSequence(l, s + i, end + 1 - i, 0);
            i = end + 1;
        }
        s += i;
        len -= i;
        PrettyLayoutChar(l, '(');
        PrettyLayoutSequence(l, s, len, tail + 1);
        PrettyLayoutChar(l, ')');
    } else
        PrettyLayoutSequence(l, s, len, tail);
    PrettyWrapLineEnd(l, PrettyCodeEnd(l->indent), tail);
}

#define PRETTY_PRIORITIES 64

/*
 * Lay out marked code spanning lines already as it is, but for lambdas,
 * which go where they land, and an initializer's rows, each starting a
 * line, as they always do
 */
static void
PrettyLayoutAsIs(PrettyWrap *l, const char *s, ptrdiff_t len)
{
    ptrdiff_t i, k, start, end, outer;

    for (i = 0; i < len; i++) {
        if (s[i] == PRETTY_GROUP && s[i + 1] == PRETTY_SHIFTED) {
            end = PrettyGroupEnd(s, i, len);
            PrettyLayoutShifted(l, s + i + 2, end - i - 2);
            i = end;
        } else if (s[i] == PRETTY_GROUP && s[i + 1] == PRETTY_ROWS) {
            end = PrettyGroupEnd(s, i, len);
            outer = l->start;
            for (k = i + 2, start = k; k <= end; k++) {
                if (k < end && s[k] == PRETTY_GROUP)
                    k = PrettyGroupEnd(s, k, end);
                else if (k == end || s[k] == PRETTY_BREAK) {
                    PrettyLayoutNewline(l, outer + 4);
                    PrettyLayoutAsIs(l, s + start, k - start);
                    start = k + 1;
                }
            }
            PrettyLayoutNewline(l, outer);
            l->drop_space = True;
            i = end;
        } else if (s[i] == PRETTY_GROUP)
            i++;
        else if (s[i] == '\n') {
            FileOutput(l->out, '\n');
            l->col = 0;
            l->spaces = 0;
        } else if (s[i] == PRETTY_NEWLINE)
            PrettyLayoutNewline(l, l->indent + 4);
        else if (s[i] != PRETTY_END && s[i] != PRETTY_BREAK)
            PrettyLayoutChar(l, s[i]);
    }
}

/*
 * Marked text without the breaks just after comments that didn't end
 * their lines in the source: printing never ends a line with one, as
 * reading the output again would then give it a line end, breaking the
 * line there and laying the rest out around that. Nor does a comment
 * end its line before a closing token, which reading it again would
 * leave unended, or before another comment: reading gives the line's
 * end to every comment in a run of them
 */
static Value
PrettyGlueNotes(Value marked)
{
    ENTER();
    const char *s = StringChars(&marked->string);
    ptrdiff_t   len = marked->string.length, i, j, k, d, at = 0, depth = 0;
    Value       out = 0;
    char        kinds[PRETTY_PARENS];

    for (i = 0; i < len; i++) {
        if (s[i] == PRETTY_END && depth > 0)
            depth--;
        if (s[i] != PRETTY_GROUP)
            continue;
        if (s[i + 1] != PRETTY_NOTE) {
            if (depth < PRETTY_PARENS)
                kinds[depth] = s[i + 1];
            depth++;
            i++;
            continue;
        }
        i = PrettyGroupEnd(s, i, len);
        /* The groups still open at the next token */
        for (j = i + 1, d = depth; j < len && (s[j] == ' ' || s[j] == PRETTY_END); j++)
            if (s[j] == PRETTY_END && d > 0)
                d--;
        if (j < len && s[j] == PRETTY_NEWLINE) {
            for (k = j + 1; k < len && (s[k] == ' ' || s[k] == PRETTY_END || s[k] == PRETTY_BREAK);
                 k++)
                ;
            /*
             * The text ends before a closing token; another comment
             * following ends the line in its place
             */
            if (k < len && !strchr(")]},;", s[k])
                && !(s[k] == PRETTY_GROUP && s[k + 1] == PRETTY_NOTE))
                continue;
        } else if (j == len
                   || s[j] != PRETTY_BREAK
                   /* An initializer's rows each start a line */
                   || (d > 0 && d <= PRETTY_PARENS && kinds[d - 1] == PRETTY_ROWS))
            continue;
        if (!out)
            out = FileStringWrite();
        FilePutsc(out, s + at, j - at);
        at = j + 1;
        /* A comment following a comment's line end needs a space instead */
        if (s[j] == PRETTY_NEWLINE && j + 2 < len && s[j + 1] == PRETTY_GROUP
            && s[j + 2] == PRETTY_NOTE)
            FileOutput(out, ' ');
    }
    if (!out)
        RETURN(marked);
    FilePutsc(out, s + at, len - at);
    RETURN(FileStringString(out));
}

/*
 * Print marked text starting at column col, with tail more columns to
 * follow it on its last line, breaking it as clang-format would to fit
 * as code; the statement holding it starts at level. Text inside
 * parentheses, as a condition, starts right after the '('. Text that
 * doesn't fit on one line breaks at its nicest places first: it lays
 * out breaking only the groups of the first priority, then of the
 * first two, and so on, until no line passes its end, or keeps the
 * layout whose lines pass the end of the line by the fewest columns,
 * then pass their ends by the fewest, and then takes the fewest lines;
 * breaking a last resort must pass the end of the line by fewer columns
 */
static void
PrettyWrapText(Value f, Value marked, int level, ptrdiff_t col, ptrdiff_t extra, ptrdiff_t tail,
               Bool paren)
{
    ENTER();
    PrettyWrap  l;
    const char *s;
    ptrdiff_t   len, i, n, k, best_past = 0, best_over = 0, best_lines = 0;
    ptrdiff_t   priorities[PRETTY_PRIORITIES];
    Bool        around = False;
    Value       best = 0;

    marked = PrettyGlueNotes(marked);
    s = StringChars(&marked->string);
    len = marked->string.length;
    /*
     * Code holding blocks other than lambdas' spans lines already; print
     * it as it is, but for lambdas, which go where they land
     */
    for (i = 0; i < len; i++) {
        if (s[i] == PRETTY_GROUP && s[i + 1] == PRETTY_SHIFTED)
            i = PrettyGroupEnd(s, i, len);
        else if (s[i] == '\n')
            break;
    }
    if (i < len) {
        PrettyWrapPass(&l, f, "", 0, level, col, extra, 0, paren, False, -1);
        PrettyLayoutAsIs(&l, s, len);
        EXIT();
        return;
    }
    /*
     * A global statement too long for its line, breaking only outside
     * brackets, where a newline would end it, breaks inside parentheses
     * put around it instead
     */
    if (level == 0 && !paren
        && ((PrettyBreaksOutside(s, len, False)
             && col + PrettyFlatWidth(s, len) + tail > PrettyCodeEnd(0))
            || PrettyBreaksOutside(s, len, True)))
        around = True;
    n = PrettyPriorities(s, len, (paren ? 1 : 0) + (around ? 1 : 0), priorities, PRETTY_PRIORITIES);
    for (k = -1; k < n; k++) {
        Value out = FileStringWrite();

        PrettyWrapPass(&l, out, s, len, level, col, extra, tail, paren, around,
                       k < 0 ? -1 : priorities[k]);
        /* Last resorts serve only to keep lines within the end of the line */
        if (!best || l.past < best_past
            || (l.past == best_past && (k < 0 || priorities[k] < PRETTY_DEPTH * PRETTY_RANKS)
                && (l.over < best_over || (l.over == best_over && l.lines < best_lines)))) {
            best = FileStringString(out);
            best_past = l.past;
            best_over = l.over;
            best_lines = l.lines;
        }
        if (l.over == 0)
            break;
    }
    if (best)
        FilePutsc(f, StringChars(&best->string), best->string.length);
    EXIT();
}

/* Print an expression as PrettyWrapText does */
static void
PrettyWrapExpr(Value f, Expr *e, int prec, int level, Bool nest, ptrdiff_t col, ptrdiff_t extra,
               ptrdiff_t tail, Bool paren)
{
    ENTER();
    Value     marked;
    Bool      was = prettyMarking;
    ptrdiff_t lambdas = prettyShiftedCount;

    if (!nest || profiling) {
        PrettyExpr(f, e, prec, level, nest);
        EXIT();
        return;
    }
    marked = FileStringWrite();
    prettyMarking = True;
    PrettyExpr(marked, e, prec, level, nest);
    prettyMarking = was;
    PrettyWrapText(f, FileStringString(marked), level, col, extra, tail, paren);
    /* Its lambdas have printed */
    prettyShiftedCount = lambdas;
    EXIT();
}

/*
 * Print the comments a control statement holds after one of its own
 * tokens, of kind keyword, following that token on its line. With own
 * set, only those that had a line of their own print, each on a line
 * at level; with inline set, only the others, so a caller can break
 * the line between them
 */
#define PRETTY_KEYWORD_ALL    0
#define PRETTY_KEYWORD_INLINE 1
#define PRETTY_KEYWORD_OWN    2

static void
PrettyKeywordComments(Value f, Expr *e, int keyword, int which, int level)
{
    ENTER();
    Expr      *c;
    CommentPtr comment;
    Value      one;
    Bool       own;
    ptrdiff_t  i;

    for (c = e->base.comments; c; c = c->tree.right) {
        comment = c->tree.left->comment.comment;
        own = comment->starts_line && comment->ends_line;
        if (comment->place != comment_keyword || comment->keyword != keyword
            || (which == PRETTY_KEYWORD_INLINE && own) || (which == PRETTY_KEYWORD_OWN && !own))
            continue;
        one = FileStringWrite();
        if (!PrettyCommentOneLine(one, comment->text, PRETTY_WIDTH))
            continue;
        if (which == PRETTY_KEYWORD_OWN) {
            FileOutput(f, '\n');
            for (i = 0; i < (ptrdiff_t)level * 4; i++)
                FileOutput(f, ' ');
        } else if (keyword != OP)
            /* Only empty parentheses hold one after their '(' */
            FileOutput(f, ' ');
        PrettyNoteText(f, comment, FileStringString(one));
    }
    EXIT();
}

/* Whether a statement holds a comment that had a line of its own after a token of kind keyword */
static Bool
PrettyKeywordOwn(Expr *e, int keyword)
{
    Expr      *c;
    CommentPtr comment;

    for (c = e->base.comments; c; c = c->tree.right) {
        comment = c->tree.left->comment.comment;
        if (comment->place == comment_keyword && comment->keyword == keyword && comment->starts_line
            && comment->ends_line)
            return True;
    }
    return False;
}

/*
 * Print a raise's exception and arguments, starting at column col,
 * breaking as a call's do, with the comments after its '(' and ')'
 */
static void
PrettyWrapRaise(Value f, Expr *e, int level, Bool nest, ptrdiff_t col)
{
    ENTER();
    Value     marked = FileStringWrite();
    Bool      was = prettyMarking;
    ptrdiff_t lambdas = prettyShiftedCount;

    prettyMarking = nest && !profiling;
    PrettyExpr(marked, e->tree.left, -1, level, nest);
    FilePuts(marked, "(");
    if (nest)
        PrettyKeywordComments(marked, e, OP, PRETTY_KEYWORD_ALL, level);
    if (e->tree.right) {
        PrettyMark(marked, PRETTY_GROUP, PRETTY_CALL);
        PrettyParameters(marked, e->tree.right, level, nest);
        PrettyMark(marked, PRETTY_END, 0);
    }
    FilePuts(marked, ")");
    if (nest)
        PrettyKeywordComments(marked, e, CP, PRETTY_KEYWORD_ALL, level);
    prettyMarking = was;
    PrettyWrapText(f, FileStringString(marked), level, col, 0, 1, False);
    prettyShiftedCount = lambdas;
    EXIT();
}

/*
 * Print a control statement's keyword, the comments following it, and
 * the '(' opening its header, returning the columns they take
 */
static ptrdiff_t
PrettyHeaderOpen(Value f, Expr *e, const char *word, int keyword, Bool nest)
{
    ENTER();
    Value     text = FileStringWrite();
    ptrdiff_t cols;

    FilePuts(text, word);
    if (nest)
        PrettyKeywordComments(text, e, keyword, PRETTY_KEYWORD_ALL, 0);
    FilePuts(text, " (");
    text = FileStringString(text);
    FilePutsc(f, StringChars(&text->string), text->string.length);
    cols = PrettyTextColumns(text);
    EXIT();
    return cols;
}

/*
 * Print the header of an if, while, for, or switch, starting at column
 * col, its continued lines extra columns further in
 */
static void
PrettyHeaderText(Value f, Expr *e, int level, Bool nest, ptrdiff_t col, ptrdiff_t extra)
{
    switch (e->base.tag) {
    case IF:
    case ELSE:
        col += PrettyHeaderOpen(f, e, "if", IF, nest);
        PrettyWrapExpr(f, e->tree.left, -1, level, nest, col, extra, 3, True);
        break;
    case WHILE:
        col += PrettyHeaderOpen(f, e, "while", WHILE, nest);
        PrettyWrapExpr(f, e->tree.left, -1, level, nest, col, extra, 3, True);
        break;
    case SWITCH:
        col += PrettyHeaderOpen(f, e, "switch", SWITCH, nest);
        PrettyWrapExpr(f, e->tree.left, -1, level, nest, col, extra, 3, True);
        break;
    case UNION:
        col += PrettyHeaderOpen(f, e, "union switch", SWITCH, nest);
        PrettyWrapExpr(f, e->tree.left, -1, level, nest, col, extra, 3, True);
        break;
    case ENUM:
        col += PrettyHeaderOpen(f, e, "enum switch", SWITCH, nest);
        PrettyWrapExpr(f, e->tree.left, -1, level, nest, col, extra, 3, True);
        break;
    case FOR: {
        /* The clauses break after their ';' as a group, as a call's arguments do */
        Value     marked = FileStringWrite();
        Bool      was = prettyMarking;
        ptrdiff_t lambdas = prettyShiftedCount;
        Expr     *clauses = e->tree.left;

        col += PrettyHeaderOpen(f, e, "for", FOR, nest);
        prettyMarking = nest && !profiling;
        PrettyMark(marked, PRETTY_GROUP, PRETTY_CLAUSES);
        if (clauses->tree.left)
            PrettyExpr(marked, clauses->tree.left, -1, level, nest);
        if (clauses->base.tag == SEMI)
            FilePuts(marked, ";");
        if (clauses->tree.right->tree.left) {
            if (clauses->base.tag == SEMI) {
                PrettyMark(marked, PRETTY_BREAK, 0);
                FilePuts(marked, " ");
            }
            PrettyExpr(marked, clauses->tree.right->tree.left, -1, level, nest);
        }
        FilePuts(marked, ";");
        if (clauses->tree.right->tree.right->tree.left) {
            PrettyMark(marked, PRETTY_BREAK, 0);
            FilePuts(marked, " ");
            PrettyExpr(marked, clauses->tree.right->tree.right->tree.left, -1, level, nest);
        }
        PrettyMark(marked, PRETTY_END, 0);
        prettyMarking = was;
        PrettyWrapText(f, FileStringString(marked), level, col, extra, 3, True);
        prettyShiftedCount = lambdas;
        break;
    }
    case TWIXT: {
        /* The enter and leave expressions break after the ';' as a for's clauses do */
        Value     marked = FileStringWrite();
        Bool      was = prettyMarking;
        ptrdiff_t lambdas = prettyShiftedCount;

        FilePuts(f, "twixt (");
        prettyMarking = nest && !profiling;
        PrettyMark(marked, PRETTY_GROUP, PRETTY_CLAUSES);
        PrettyExpr(marked, e->tree.left->tree.left, -1, level, nest);
        FilePuts(marked, ";");
        PrettyMark(marked, PRETTY_BREAK, 0);
        FilePuts(marked, " ");
        PrettyExpr(marked, e->tree.left->tree.right, -1, level, nest);
        PrettyMark(marked, PRETTY_END, 0);
        prettyMarking = was;
        PrettyWrapText(f, FileStringString(marked), level, col + 7, extra, 3, True);
        prettyShiftedCount = lambdas;
        break;
    }
    }
    FilePuts(f, ")");
}

/*
 * Print the header of a control statement whose body is body, starting
 * at column col, returning whether it wrapped. A wrapped header before
 * a body without braces continues four columns further in, so its
 * lines don't line up with the body. Comments after its ')' follow it,
 * and those that had lines of their own keep them, at the body's level,
 * a block's '{' then going on a line of its own as after a wrapped
 * header
 */
static Bool
PrettyControlHeader(Value f, Expr *e, Expr *body, int level, Bool nest, ptrdiff_t col)
{
    ENTER();
    Value text = FileStringWrite();
    Bool  wrapped;

    PrettyHeaderText(text, e, level, nest, col, 0);
    text = FileStringString(text);
    wrapped = PrettyMultiline(text);
    if (wrapped && body && body->base.tag != OC) {
        text = FileStringWrite();
        PrettyHeaderText(text, e, level, nest, col, 4);
        text = FileStringString(text);
    }
    FilePutsc(f, StringChars(&text->string), text->string.length);
    if (nest) {
        Bool block = !body || body->base.tag == OC;

        PrettyKeywordComments(f, e, CP, PRETTY_KEYWORD_INLINE, level);
        if (PrettyKeywordOwn(e, CP)) {
            PrettyKeywordComments(f, e, CP, PRETTY_KEYWORD_OWN, block ? level : level + 1);
            if (block)
                wrapped = True;
        }
    }
    EXIT();
    return wrapped;
}

/*
 * Print an if, with any else, starting after its indentation; an else
 * holding another if continues the chain on the same level, as
 * '} else if (...) {'
 */
static void
PrettyIf(Value f, Expr *e, int level, Bool nest)
{
    Bool      closed, wrapped;
    Expr     *other;
    ptrdiff_t col = (ptrdiff_t)level * 4;

    for (;;) {
        if (e->base.tag == IF) {
            wrapped = PrettyControlHeader(f, e, e->tree.right, level, nest, col);
            if (PrettyBraceBody(f, e->tree.right, level, nest, wrapped))
                FilePuts(f, "\n");
            return;
        }
        wrapped = PrettyControlHeader(f, e, e->tree.right->tree.left, level, nest, col);
        closed = PrettyBraceBody(f, e->tree.right->tree.left, level, nest, wrapped);
        /*
         * Comments after the '}' follow it, and those that had lines of
         * their own keep them, the else then starting a line
         */
        if (closed && nest) {
            Value text = FileStringWrite();

            PrettyKeywordComments(text, e, CC, PRETTY_KEYWORD_INLINE, level);
            text = FileStringString(text);
            FilePutsc(f, StringChars(&text->string), text->string.length);
            col = (ptrdiff_t)level * 4 + 1 + PrettyTextColumns(text);
            if (PrettyKeywordOwn(e, CC)) {
                PrettyKeywordComments(f, e, CC, PRETTY_KEYWORD_OWN, level);
                FilePuts(f, "\n");
                closed = False;
            }
        } else
            col = (ptrdiff_t)level * 4 + 1;
        if (closed)
            FilePuts(f, " else");
        else {
            PrettyIndent(f, 0, level);
            FilePuts(f, "else");
            col = (ptrdiff_t)level * 4;
        }
        if (nest) {
            Value text = FileStringWrite();

            PrettyKeywordComments(text, e, ELSE, PRETTY_KEYWORD_ALL, level);
            text = FileStringString(text);
            FilePutsc(f, StringChars(&text->string), text->string.length);
            col += PrettyTextColumns(text);
        }
        /* '} else if (' or 'else if (', after any comments */
        col += closed ? 6 : 5;
        other = e->tree.right->tree.right;
        if (other->base.tag != IF && other->base.tag != ELSE) {
            if (PrettyBraceBody(f, other, level, nest, False))
                FilePuts(f, "\n");
            return;
        }
        FileOutput(f, ' ');
        e = other;
    }
}

void
PrettyStatement(Value f, Expr *e, int level, int blevel, Bool nest)
{
    switch (e->base.tag) {
    case EXPR:
        PrettyIndent(f, e, level);
        /* An assignment's value may move past its operator */
        if (PrettyAssignment(e->tree.left->base.tag))
            prettyAssign = e->tree.left;
        PrettyWrapExpr(f, e->tree.left, -1, level, nest, (ptrdiff_t)level * 4, 0, 1, False);
        prettyAssign = 0;
        FilePuts(f, ";\n");
        break;
    case IF:
    case ELSE:
        PrettyIndent(f, e, level);
        if (nest)
            PrettyIf(f, e, level, nest);
        else {
            PrettyHeaderText(f, e, level, nest, (ptrdiff_t)level * 4, 0);
            FilePuts(f, "\n");
        }
        break;
    case WHILE:
        PrettyIndent(f, e, level);
        if (!nest) {
            PrettyHeaderText(f, e, level, nest, (ptrdiff_t)level * 4, 0);
            FilePuts(f, "\n");
        } else if (PrettyBraceBody(
                       f, e->tree.right, level, nest,
                       PrettyControlHeader(f, e, e->tree.right, level, nest, (ptrdiff_t)level * 4)))
            FilePuts(f, "\n");
        break;
    case COMMENT_CONST:
        if (nest && e->comment.comment->style != comment_blank) {
            PrettyComment(f, e->comment.comment->text, level);
            FileOutput(f, '\n');
        }
        break;
    case OC:
        PrettyIndent(f, 0, blevel);
        FilePuts(f, "{\n");
        PrettyBlock(f, e, blevel + 1, nest, False);
        PrettyIndent(f, 0, blevel);
        FilePuts(f, "}\n");
        break;
    case DO:
        PrettyIndent(f, 0, level);
        FilePuts(f, "do");
        if (nest)
            PrettyKeywordComments(f, e, DO, PRETTY_KEYWORD_ALL, level);
        if (nest && PrettyBraceBody(f, e->tree.left, level, nest, False)) {
            ptrdiff_t col = (ptrdiff_t)level * 4 + 1;
            Value     text = FileStringWrite();

            /* Comments after the '}' follow it, or keep their lines, the while then starting one */
            PrettyKeywordComments(text, e, CC, PRETTY_KEYWORD_INLINE, level);
            text = FileStringString(text);
            FilePutsc(f, StringChars(&text->string), text->string.length);
            col += PrettyTextColumns(text) + 1;
            if (PrettyKeywordOwn(e, CC)) {
                PrettyKeywordComments(f, e, CC, PRETTY_KEYWORD_OWN, level);
                FilePuts(f, "\n");
                PrettyIndent(f, 0, level);
                col = (ptrdiff_t)level * 4;
            } else
                FileOutput(f, ' ');
            col += PrettyHeaderOpen(f, e, "while", WHILE, nest);
            PrettyWrapExpr(f, e->tree.right, -1, level, nest, col, 0, 2, True);
        } else {
            if (!nest)
                FilePuts(f, "\n");
            PrettyIndent(f, e, level);
            PrettyWrapExpr(f, e->tree.right, -1, level, nest,
                           (ptrdiff_t)level * 4 + PrettyHeaderOpen(f, e, "while", WHILE, nest), 0,
                           2, True);
        }
        FilePuts(f, ")");
        if (nest)
            PrettyKeywordComments(f, e, CP, PRETTY_KEYWORD_ALL, level);
        FilePuts(f, ";\n");
        break;
    case FOR:
        PrettyIndent(f, e, level);
        if (!nest) {
            PrettyHeaderText(f, e, level, nest, (ptrdiff_t)level * 4, 0);
            FilePuts(f, "\n");
        } else if (PrettyBraceBody(
                       f, e->tree.right, level, nest,
                       PrettyControlHeader(f, e, e->tree.right, level, nest, (ptrdiff_t)level * 4)))
            FilePuts(f, "\n");
        break;
    case SWITCH:
    case UNION:
    case ENUM:
        PrettyIndent(f, e, level);
        if (nest) {
            ExprPtr block = e->tree.right, prev;
            Bool    cuddled = True, blank = False;

            /* The case list's brace goes on a line of its own after a wrapped header */
            if (PrettyControlHeader(f, e, 0, level, nest, (ptrdiff_t)level * 4)) {
                FilePuts(f, "\n");
                PrettyIndent(f, 0, level);
                FilePuts(f, "{\n");
                cuddled = False;
            } else
                FilePuts(f, " {\n");
            for (prev = 0; block; block = block->tree.right) {
                /*
                 * Comments before a case label print at the label's level,
                 * a run of them with a blank line above, after a case or
                 * the switch's '{' ending its header's line; a blank line
                 * from the source stays, except just inside the '{'
                 */
                if (PrettyBlankEntry(block)) {
                    if (prev && !blank)
                        FilePuts(f, "\n");
                    blank = prev != 0;
                    continue;
                }
                if (block->tree.left->base.tag == COMMENT_CONST) {
                    if (!blank && (prev ? prev->tree.left->base.tag != COMMENT_CONST : cuddled))
                        FilePuts(f, "\n");
                    PrettyStatement(f, block->tree.left, level, level, nest);
                    prev = block;
                    blank = False;
                    continue;
                }
                prev = block;
                blank = False;
                PrettyIndent(f, 0, level);
                if (block->tree.left->tree.left) {
                    ExprPtr label = block->tree.left->tree.left;

                    FilePuts(f, "case ");
                    /* A union case names the member, then any name bound to its value */
                    if (e->base.tag != SWITCH) {
                        PrettyExpr(f, label->tree.left, -1, level, nest);
                        if (label->tree.right) {
                            FileOutput(f, ' ');
                            PrettyExpr(f, label->tree.right, -1, level, nest);
                        }
                    } else
                        PrettyExpr(f, label, -1, level, nest);
                } else
                    FilePuts(f, "default");
                FilePuts(f, ":");
                PrettyCaseBody(f, block->tree.left->tree.right, level, nest);
            }
            PrettyIndent(f, 0, level);
            FilePuts(f, "}");
        } else
            PrettyHeaderText(f, e, level, nest, (ptrdiff_t)level * 4, 0);
        FilePuts(f, "\n");
        break;
    case SEMI:
        PrettyIndent(f, e, level);
        FilePuts(f, ";\n");
        break;
    case BREAK:
        PrettyIndent(f, e, level);
        FilePuts(f, "break");
        if (nest)
            PrettyKeywordComments(f, e, BREAK, PRETTY_KEYWORD_ALL, level);
        FilePuts(f, ";\n");
        break;
    case CONTINUE:
        PrettyIndent(f, e, level);
        FilePuts(f, "continue");
        if (nest)
            PrettyKeywordComments(f, e, CONTINUE, PRETTY_KEYWORD_ALL, level);
        FilePuts(f, ";\n");
        break;
    case RETURNTOK:
        PrettyIndent(f, e, level);
        /* A void function returns no value */
        if (e->tree.right) {
            Value value = FileStringWrite();

            PrettyWrapExpr(value, e->tree.right, -1, level, nest, (ptrdiff_t)level * 4 + 7, 0, 1,
                           False);
            value = FileStringString(value);
            /* A value can start a line of its own, as a lambda does */
            FilePuts(f, "return");
            if (value->string.length == 0 || StringChars(&value->string)[0] != '\n')
                FileOutput(f, ' ');
            FilePutsc(f, StringChars(&value->string), value->string.length);
        } else {
            FilePuts(f, "return");
            if (nest)
                PrettyKeywordComments(f, e, RETURNTOK, PRETTY_KEYWORD_ALL, level);
        }
        FilePuts(f, ";\n");
        break;
    case FUNC:
        PrettyIndent(f, e, level);
        {
            DeclListPtr decl = e->decl.decl;
            ExprPtr     init = decl->init;
            if (init) {
                CodePtr code = init->code.code;

                PrettyCode(f, code, decl->name, e->decl.class, e->decl.publish, level, nest);
            } else {
                Type *t = e->decl.type;
                FilePrintf(f, "%p%k%T ", e->decl.publish, e->decl.class, t->func.ret);
                if (nest)
                    PrettyCommentList(f, decl->comments, comment_before);
                FilePrintf(f, "%A", decl->name);
                if (nest)
                    PrettyCommentList(f, decl->comments, comment_after);
                PrettyArgs(f, t->func.args, level, nest);
                if (nest)
                    PrettyDocSemicolon(f, decl->comments, level);
                else
                    FilePuts(f, ";");
            }
        }
        FilePuts(f, "\n");
        break;
    case TYPEDEF:
        PrettyTypedef(f, e->tree.left, level, nest);
        break;
    case VAR:
        PrettyIndent(f, e, level);
        PrettyDecl(f, e, level, nest);
        FilePuts(f, ";\n");
        break;
    case NAMESPACE:
        PrettyIndent(f, e, level);
        FilePrintf(f, "%pnamespace %A", e->tree.left->decl.publish, e->tree.left->decl.decl->name);
        if (PrettyBraceBody(f, e->tree.right, level, nest, False))
            FilePuts(f, "\n");
        break;
    case EXCEPTION: {
        SymbolPtr symbol = e->decl.decl->symbol;

        Value     prefix = FileStringWrite();
        Bool      doc = (symbol && symbol->exception.doc != Void)
            || (nest && PrettyHasDoc(e->decl.decl->comments));

        PrettyIndent(f, e, level);
        FilePrintf(prefix, "%pexception %A", e->decl.publish, e->decl.decl->name);
        prefix = FileStringString(prefix);
        FilePutsc(f, StringChars(&prefix->string), prefix->string.length);
        if (nest && !profiling)
            PrettyWrapArgs(f, e->decl.type->func.args, level,
                           (ptrdiff_t)level * 4 + PrettyTextColumns(prefix), doc ? 0 : 1);
        else
            PrettyArgs(f, e->decl.type->func.args, level, nest);
        if (symbol && symbol->exception.doc != Void) {
            FilePuts(f, "\n");
            PrettyComment(f, symbol->exception.doc, level + 1);
        }
        if (nest)
            PrettyDocSemicolon(f, e->decl.decl->comments, level);
        else
            FilePuts(f, ";");
        FilePuts(f, "\n");
        break;
    }
    case IMPORT:
        PrettyIndent(f, e, level);
        FilePrintf(f, "%pimport ", e->tree.right->decl.publish);
        PrettyExpr(f, e->tree.left, -1, level, nest);
        FilePuts(f, ";\n");
        break;
    case TWIXT:
        PrettyIndent(f, e, level);
        if (!nest) {
            PrettyHeaderText(f, e, level, nest, (ptrdiff_t)level * 4, 0);
            FilePuts(f, "\n");
        } else if (PrettyBraceBody(f, e->tree.right->tree.left, level, nest,
                                   PrettyControlHeader(f, e, e->tree.right->tree.left, level, nest,
                                                       (ptrdiff_t)level * 4)))
            FilePuts(f, "\n");
        break;
    case CATCH: {
        Bool closed = False;

        PrettyIndent(f, e, level);
        FilePuts(f, "try");
        if (nest)
            closed = PrettyBraceBody(f, e->tree.right, level, nest, False);
        else
            FilePuts(f, " ");
        PrettyCatch(f, e->tree.left, level, nest, &closed);
        if (nest)
            FilePuts(f, "\n");
        break;
    }
    case RAISE:
        PrettyIndent(f, e, level);
        /* The exception may be a name qualified by namespaces */
        {
            Value text = FileStringWrite();

            FilePuts(text, "raise");
            if (nest)
                PrettyKeywordComments(text, e, RAISE, PRETTY_KEYWORD_ALL, level);
            FileOutput(text, ' ');
            text = FileStringString(text);
            FilePutsc(f, StringChars(&text->string), text->string.length);
            PrettyWrapRaise(f, e, level, nest, (ptrdiff_t)level * 4 + PrettyTextColumns(text));
        }
        FilePuts(f, ";\n");
        break;
    case DOLLAR:
        /* A top-level expression, which prints its value, with any base after '#' */
        PrettyIndent(f, e, level);
        PrettyWrapExpr(f, e->tree.left, -1, level, nest, (ptrdiff_t)level * 4, 0, 0, False);
        if (e->tree.right) {
            FilePuts(f, " # ");
            PrettyExpr(f, e->tree.right, -1, level, nest);
        }
        FilePuts(f, "\n");
        break;
    case COMMAND:
        /* A top-level command, as formatting keeps it */
        PrettyIndent(f, e, level);
        FilePuts(f, AtomName(e->tree.left->atom.atom));
        for (Expr *a = e->tree.right; a; a = a->tree.right) {
            Expr *arg = a->tree.left;

            FilePuts(f, a == e->tree.right ? " " : ", ");
            /* A namespace name reads as an array of its parts */
            if (arg->base.tag == POLY_CONST && ValueIsArray(arg->constant.constant)) {
                Value     parts = arg->constant.constant;
                ptrdiff_t i;

                for (i = 0; i < ArrayLimits(&parts->array)[0]; i++) {
                    Value part = ArrayValueGet(&parts->array, i);

                    if (i)
                        FilePuts(f, "::");
                    FilePutsc(f, StringChars(&part->string), part->string.length);
                }
            } else
                PrettyExpr(f, arg, PrettyPrecedence(COMMA) + 1, level, nest);
        }
        FilePuts(f, "\n");
        break;
    }
}

/*
 * Print leading comment entries, returning the rest of the list. A
 * blank line from the source prints between them and after them, once
 */
static Expr *
PrettyLeadingComments(Value f, Expr *c, int level)
{
    Bool blank = True;

    for (; c && c->tree.left && c->tree.left->comment.comment->place != comment_trailing;
         c = c->tree.right) {
        if (PrettyBlankEntry(c)) {
            if (!blank)
                FileOutput(f, '\n');
            blank = True;
            continue;
        }
        PrettyComment(f, c->tree.left->comment.comment->text, level);
        FileOutput(f, '\n');
        blank = False;
    }
    return c;
}

/* Columns of the last line of text, ignoring a final newline */
static ptrdiff_t
PrettyTextColumns(Value text)
{
    const char *s = StringChars(&text->string);
    ptrdiff_t   len = text->string.length;

    if (len && s[len - 1] == '\n')
        len--;
    return PrettyLastLineColumns(s, len);
}

/* Whether text spans more than one line */
static Bool
PrettyMultiline(Value text)
{
    const char *s = StringChars(&text->string);
    ptrdiff_t   len = text->string.length, i;

    for (i = 0; i < len - 1; i++)
        if (s[i] == '\n')
            return True;
    return False;
}

/* Whether comment entries, after their leading ones, hold a trailing one */
static Bool
PrettyHasTrailing(Expr *c)
{
    for (; c && c->tree.left; c = c->tree.right)
        if (c->tree.left->comment.comment->place == comment_trailing)
            return True;
    return False;
}

/* Whether comment entries start with a leading one, apart from blank lines */
static Bool
PrettyHasLeading(Expr *c)
{
    while (PrettyBlankEntry(c))
        c = c->tree.right;
    return c && c->tree.left && c->tree.left->comment.comment->place != comment_trailing
        && c->tree.left->comment.comment->place != comment_doc;
}

/*
 * Whether the trailing comments in comment entries all fit after the
 * last line of text, each on one line, as PrettyTrailingComments puts
 * them there
 */
static Bool
PrettyTrailingFits(Value text, Expr *c)
{
    ENTER();
    ptrdiff_t len = text->string.length, col;

    if (len && StringChars(&text->string)[len - 1] == '\n')
        len--;
    col = PrettyLastLineColumns(StringChars(&text->string), len);

    for (; c && c->tree.left; c = c->tree.right) {
        Value one = FileStringWrite();

        if (c->tree.left->comment.comment->place != comment_trailing)
            continue;
        if (!PrettyCommentOneLine(one, c->tree.left->comment.comment->text,
                                  PRETTY_LINE_END - col - 1)) {
            EXIT();
            return False;
        }
        one = FileStringString(one);
        col += 1 + StringLength(StringChars(&one->string), one->string.length);
    }
    EXIT();
    return True;
}

/*
 * The text of a declaration or member line, its type padded to width;
 * paren puts a global declaration in parentheses, the '(' taking a
 * column of the padding
 */
static Value
PrettyMemberLine(PrettyMember *m, int level, ptrdiff_t width, Bool paren)
{
    ENTER();
    Value line = FileStringWrite();

    PrettyIndent(line, m->stat, level);
    if (paren)
        FileOutput(line, '(');
    FilePutsc(line, StringChars(&m->type->string), m->type->string.length);
    if (!PrettyMultiline(m->type)) {
        ptrdiff_t pad;

        for (pad = PrettyTextColumns(m->type) + (paren ? 1 : 0); pad < width; pad++)
            FileOutput(line, ' ');
    }
    FileOutput(line, ' ');
    if (m->decl) {
        Value prefix = FileStringString(line);

        line = FileStringWrite();
        FilePutsc(line, StringChars(&prefix->string), prefix->string.length);
        PrettyWrapDeclNames(line, m->decl, level, m->nest, PrettyTextColumns(prefix), paren);
        if (paren)
            FileOutput(line, ')');
        FileOutput(line, ';');
    } else
        FilePutsc(line, StringChars(&m->rest->string), m->rest->string.length);
    RETURN(FileStringString(line));
}

/*
 * Print lines of code at level, as clang-format aligns them. In each
 * run of declarations or members, which comments don't interrupt, the
 * names line up one space past the widest type printed on one line.
 * Trailing comments on consecutive lines line up one space past the
 * longest of those lines, or follow their own line when they don't fit
 * there; a line without one, a comment on its own line, or a blank
 * line starts a new group. Blank lines from the source stay, apart from
 * one before the first line. The last line ends with a newline only
 * when newline is set
 */
static void
PrettyMembers(Value f, PrettyMember *m, ptrdiff_t n, int level, Bool newline)
{
    ENTER();
    ptrdiff_t  i, j, k, width;
    Bool       paren, left;
    Value     *lines = AllocateTemp((size_t)n * sizeof(Value));
    ptrdiff_t *columns = AllocateTemp((size_t)n * sizeof(ptrdiff_t));
    Bool      *trails = AllocateTemp((size_t)n * sizeof(Bool));

    for (i = 0; i < n; i++) {
        lines[i] = m[i].text;
        columns[i] = 0;
        /* A blank line in the source stays, except before the first line */
        if (PrettyBlankEntry(m[i].comments)) {
            m[i].comments = m[i].comments->tree.right;
            if (i > 0)
                m[i].blank = True;
        }
    }
    for (i = 0; i < n; i = j) {
        if (!m[i].type) {
            j = i + 1;
            continue;
        }
        width = 0;
        for (j = i; j < n && (m[j].type || !m[j].text); j++)
            if (m[j].type && !PrettyMultiline(m[j].type)) {
                ptrdiff_t w = PrettyTextColumns(m[j].type);

                if (w > width)
                    width = w;
            }
        /*
         * A one-line definition that the run's padding would carry past
         * the end of the line leaves the run, which then lines up again
         */
        for (k = i, left = False; k < j; k++)
            if (m[k].type && m[k].one_line
                && !PrettyCodeFits((ptrdiff_t)level * 4,
                                   (ptrdiff_t)level * 4 + width + 1
                                       + PrettyTextColumns(m[k].rest))) {
                m[k].text = lines[k] = PrettyMemberLine(&m[k], level, 0, False);
                m[k].type = 0;
                left = True;
            }
        if (left) {
            j = i;
            continue;
        }
        for (k = i; k < j; k++) {
            if (!m[k].type)
                continue;
            /*
             * A global declaration of several names too long for its line
             * goes in parentheses, so its names can break where a newline
             * would otherwise end it, as does one whose lines pass the
             * end of the line fewer times so, its values moving past
             * their '=' as in a function
             */
            paren = level == 0 && m[k].decl && !PrettyMultiline(m[k].type)
                && (PrettyDeclBreaks(m[k].decl, m[k].nest)
                    || (m[k].decl->decl.decl->next
                        && !PrettyCodeFits(
                            0, width + 1 + PrettyDeclNamesWidth(m[k].decl, level, m[k].nest) + 1)));
            lines[k] = PrettyMemberLine(&m[k], level, width, paren);
            if (level == 0 && m[k].decl && !paren && !PrettyMultiline(m[k].type)) {
                ptrdiff_t lines_count, over, paren_over;
                Value     in_paren;

                PrettyTextFit(lines[k], 0, 0, &lines_count, &over);
                if (over > 0) {
                    in_paren = PrettyMemberLine(&m[k], level, width, True);
                    PrettyTextFit(in_paren, 0, 0, &lines_count, &paren_over);
                    if (paren_over < over)
                        lines[k] = in_paren;
                }
            }
        }
    }
    /*
     * Trailing comments that don't fit after their line go before it,
     * as leading ones do, blank line and all
     */
    for (i = 0; i < n; i++) {
        trails[i] = lines[i] && PrettyHasTrailing(m[i].comments)
            && PrettyTrailingFits(lines[i], m[i].comments);
        if (lines[i] && PrettyHasTrailing(m[i].comments) && !trails[i] && m[i].spaced)
            m[i].blank = True;
    }
    for (i = 0; i < n; i = j) {
        ptrdiff_t column = 0;

        if (!trails[i]) {
            j = i + 1;
            continue;
        }
        for (j = i; j < n && trails[j]
             && (j == i
                 || (!m[j].blank && !PrettyHasLeading(m[j].comments)
                     && !PrettyMultiline(lines[j])));
             j++) {
            ptrdiff_t w = PrettyTextColumns(lines[j]) + 1;

            if (w > column)
                column = w;
        }
        for (k = i; k < j; k++)
            columns[k] = column;
    }
    for (i = 0; i < n; i++) {
        Expr *c;

        if (m[i].blank)
            FileOutput(f, '\n');
        c = PrettyLeadingComments(f, m[i].comments, level);

        if (lines[i])
            PrettyTrailingComments(f, lines[i], c, level, newline || i < n - 1, columns[i]);
    }
    EXIT();
}

/*
 * Print a parameter's type, poly included, as declarations need it,
 * with its comments when comments is set
 */
static void
PrettyArgType(Value f, ArgType *arg, Bool comments)
{
    ENTER();
    Value type = FileStringWrite();

    if (comments) {
        PrettyTypeLine(type, arg->type);
        PrettyTypeText(f, FileStringString(type), arg->comments);
    } else
        FilePutType(f, arg->type, False);
    EXIT();
}

/*
 * Print a parameter with its type, poly included, as declarations need
 * it and as it was written
 */
static void
PrettyArg(Value f, ArgType *arg, Bool comments)
{
    PrettyArgType(f, arg, comments);
    if (arg->name) {
        FileOutput(f, ' ');
        FilePuts(f, AtomName(arg->name));
    }
    if (arg->varargs)
        FilePuts(f, " ...");
}

/*
 * A parameter's comments past those among its type's tokens, apart
 * from a blank line before the first, which no layout prints
 */
static Expr *
PrettyArgComments(ArgType *arg, ArgType *args)
{
    Expr *c = PrettyPastType(arg->comments);

    if (arg == args && PrettyBlankEntry(c))
        c = PrettyPastType(c->tree.right);
    return c;
}

/*
 * Print a function's parameters: on one line, or, when comments is set
 * and any parameter has comments, one parameter per line with its
 * comments, aligned as PrettyMembers does
 */
static void
PrettyArgs(Value f, ArgType *args, int level, Bool comments)
{
    ENTER();
    ArgType      *arg = 0;
    PrettyMember *m;
    ptrdiff_t     n, i;

    if (comments)
        for (arg = args; arg && !PrettyArgComments(arg, args); arg = arg->next)
            ;
    if (!arg) {
        FilePuts(f, "(");
        for (; args; args = args->next) {
            PrettyArg(f, args, comments);
            if (args->next)
                FilePuts(f, ", ");
        }
        FilePuts(f, ")");
        EXIT();
        return;
    }
    for (n = 0, arg = args; arg; arg = arg->next)
        n++;
    m = AllocateTemp((size_t)n * sizeof(PrettyMember));
    for (i = 0, arg = args; arg; arg = arg->next, i++) {
        Value type = FileStringWrite();
        Value rest = FileStringWrite();

        m[i].comments = PrettyPastType(arg->comments);
        m[i].text = 0;
        m[i].stat = 0;
        m[i].decl = 0;
        m[i].blank = False;
        m[i].spaced = False;
        m[i].one_line = False;
        PrettyArgType(type, arg, True);
        if (arg->name)
            FilePuts(rest, AtomName(arg->name));
        if (arg->varargs)
            FilePuts(rest, arg->name ? " ..." : "...");
        if (arg->next)
            FileOutput(rest, ',');
        m[i].type = FileStringString(type);
        m[i].rest = FileStringString(rest);
    }
    FilePuts(f, "(\n");
    PrettyMembers(f, m, n, level + 1, False);
    FilePuts(f, ")");
    EXIT();
}

/*
 * Print a parameter list starting at column col, with tail more columns
 * following it on its line: one per line with their comments when any
 * has comments, and otherwise breaking as a call's arguments do, and
 * before each parameter starting a line in the source
 */
static void
PrettyWrapArgs(Value f, ArgType *args, int level, ptrdiff_t col, ptrdiff_t tail)
{
    ENTER();
    ArgType *arg;
    Value    marked;
    Bool     was = prettyMarking;

    for (arg = args; arg && !PrettyArgComments(arg, args); arg = arg->next)
        ;
    if (arg) {
        PrettyArgs(f, args, level, True);
        EXIT();
        return;
    }
    marked = FileStringWrite();
    prettyMarking = True;
    FilePuts(marked, "(");
    PrettyMark(marked, PRETTY_GROUP, PRETTY_CALL);
    for (arg = args; arg; arg = arg->next) {
        if (arg != args && arg->starts_line)
            PrettyMark(marked, PRETTY_NEWLINE, 0);
        PrettyArg(marked, arg, True);
        if (arg->next) {
            FilePuts(marked, ", ");
            PrettyMark(marked, PRETTY_BREAK, 0);
        }
    }
    PrettyMark(marked, PRETTY_END, 0);
    FilePuts(marked, ")");
    prettyMarking = was;
    PrettyWrapText(f, FileStringString(marked), level, col, 0, tail, False);
    EXIT();
}

/*
 * The comment entries of the members declared together from first up
 * to end, each once (a declaration's comments go to all its names),
 * leading ones before trailing ones
 */
static Expr *
PrettyGroupComments(StructType *st, ptrdiff_t first, ptrdiff_t end)
{
    ENTER();
    Expr     *group = NewExprTree(OC, 0, 0), **last = &group;
    int       pass;
    ptrdiff_t k;

    if (end - first == 1)
        RETURN(StructMemComments(st, StructTypeAtoms(st)[first]));
    for (pass = 0; pass < 2; pass++) {
        for (k = first; k < end; k++) {
            Expr *c = StructMemComments(st, StructTypeAtoms(st)[k]);

            for (; c && c->tree.left; c = c->tree.right) {
                CommentPtr comment = c->tree.left->comment.comment;
                Expr      *g;

                if ((comment->place == comment_trailing) != (pass == 1))
                    continue;
                for (g = group; g->tree.left && g->tree.left->comment.comment != comment;
                     g = g->tree.right)
                    ;
                if (g->tree.left)
                    continue;
                *last = NewExprTree(OC, c->tree.left, *last);
                last = &(*last)->tree.right;
            }
        }
    }
    RETURN(group);
}

static ptrdiff_t
PrettyAbs(ptrdiff_t v)
{
    return v < 0 ? -v : v;
}

/* Whether a struct or union's members all come from one declaration */
static Bool
PrettyOneDeclaration(StructType *st)
{
    ptrdiff_t i;

    for (i = 1; i < st->nelements; i++)
        if (!StructTypeGrouped(st)[i])
            return False;
    return True;
}

/*
 * Print an enum's values one level in as a grid of aligned columns,
 * row by row. When some count of columns that fits as code divides the
 * number of values, the grid is a rectangle, the one most nearly
 * square, wider rather than taller; otherwise it has as many columns
 * as fit, and a short last row
 */
static Bool
PrettyEnumGrid(Value f, StructType *st, int level)
{
    ENTER();
    ptrdiff_t n = st->nelements, i, c, rows, columns = 0, width = 0;

    for (i = 0; i < n; i++) {
        const char *name = AtomName(StructTypeAtoms(st)[i]);
        ptrdiff_t   w = PrettyColumns(name, (ptrdiff_t)strlen(name)) + 1;

        if (w > width)
            width = w;
    }
    for (c = 2; c < n; c++) {
        if (n % c != 0
            || !PrettyCodeFits((ptrdiff_t)level * 4, (ptrdiff_t)level * 4 + c * (width + 1) - 1))
            continue;
        rows = n / c;
        if (!columns || PrettyAbs(c - rows) < PrettyAbs(columns - n / columns)
            || (PrettyAbs(c - rows) == PrettyAbs(columns - n / columns) && c > columns))
            columns = c;
    }
    /* Otherwise, as many columns as fit, with a short last row */
    if (!columns)
        for (columns = 1; PrettyCodeFits((ptrdiff_t)level * 4,
                                         (ptrdiff_t)level * 4 + (columns + 1) * (width + 1) - 1)
             && columns + 1 < n;
             columns++)
            ;
    for (i = 0; i < n; i++) {
        const char *name = AtomName(StructTypeAtoms(st)[i]);
        ptrdiff_t   w = PrettyColumns(name, (ptrdiff_t)strlen(name));

        if (i % columns == 0)
            PrettyIndent(f, 0, level);
        FilePuts(f, name);
        if (i < n - 1) {
            FileOutput(f, ',');
            w++;
        }
        if (i % columns == columns - 1 || i == n - 1)
            FileOutput(f, '\n');
        else
            while (w++ < width + 1)
                FileOutput(f, ' ');
    }
    EXIT();
    return True;
}

/*
 * Print an enum's values one level in: as a grid when none has
 * comments and PrettyEnumGrid finds one, and otherwise filling lines to
 * the code width. A value with leading comments starts a line after
 * them, and one with trailing comments ends its line with them; one
 * after a blank line in the source starts a line after a blank line
 */
static void
PrettyEnumValues(Value f, StructType *st, int level)
{
    ENTER();
    ptrdiff_t i;
    Value     line = 0;

    if (!st->comments && PrettyEnumGrid(f, st, level)) {
        EXIT();
        return;
    }

    for (i = 0; i < st->nelements; i++) {
        Expr     *c = StructMemComments(st, StructTypeAtoms(st)[i]);
        Value     value = FileStringWrite();
        ptrdiff_t width;

        FilePuts(value, AtomName(StructTypeAtoms(st)[i]));
        if (i < st->nelements - 1)
            FileOutput(value, ',');
        value = FileStringString(value);
        width = PrettyTextColumns(value);
        if (c && c->tree.left && c->tree.left->comment.comment->place != comment_trailing && line) {
            FilePutsc(f, StringChars(&FileStringString(line)->string),
                      FileStringString(line)->string.length);
            FileOutput(f, '\n');
            line = 0;
        }
        /* A blank line from the source stays, except before the first value */
        if (PrettyBlankEntry(c)) {
            if (i > 0)
                FileOutput(f, '\n');
            c = c->tree.right;
        }
        c = PrettyLeadingComments(f, c, level);
        if (line
            && !PrettyCodeFits((ptrdiff_t)level * 4,
                               PrettyTextColumns(FileStringString(line)) + 1 + width)) {
            Value text = FileStringString(line);

            FilePutsc(f, StringChars(&text->string), text->string.length);
            FileOutput(f, '\n');
            line = 0;
        }
        if (line)
            FileOutput(line, ' ');
        else {
            line = FileStringWrite();
            PrettyIndent(line, 0, level);
        }
        FilePutsc(line, StringChars(&value->string), value->string.length);
        if (PrettyHasTrailing(c)) {
            PrettyTrailingComments(f, FileStringString(line), c, level, True, 0);
            line = 0;
        }
    }
    if (line) {
        Value text = FileStringString(line);

        FilePutsc(f, StringChars(&text->string), text->string.length);
        FileOutput(f, '\n');
    }
    EXIT();
}

/* Print a comment on one line, marking it printed, when it fits there */
static void
PrettyInlineComment(Value f, CommentPtr comment, const char *before, const char *after)
{
    ENTER();
    Value one = FileStringWrite();

    if (comment->style != comment_blank && PrettyCommentOneLine(one, comment->text, PRETTY_WIDTH)) {
        one = FileStringString(one);
        FilePuts(f, before);
        FilePutsc(f, StringChars(&one->string), one->string.length);
        FilePuts(f, after);
        comment->printed = True;
    }
    EXIT();
}

/*
 * Print a function type's parameters on one line, with their comments.
 * Those leading a parameter after the first go before its comma, as
 * one after the comma would trail the parameter before it
 */
static void
PrettyTypeArgs(Value f, ArgType *args)
{
    ArgType *arg;
    Expr    *c, *next;

    FilePuts(f, "(");
    for (c = PrettyPastType(args ? args->comments : 0);
         c && c->tree.left && c->tree.left->comment.comment->place != comment_trailing;
         c = c->tree.right)
        PrettyInlineComment(f, c->tree.left->comment.comment, "", " ");
    for (arg = args; arg; arg = arg->next) {
        PrettyArg(f, arg, True);
        for (; c && c->tree.left; c = c->tree.right)
            PrettyInlineComment(f, c->tree.left->comment.comment, " ", "");
        if (!arg->next)
            break;
        for (next = PrettyPastType(arg->next->comments);
             next && next->tree.left && next->tree.left->comment.comment->place != comment_trailing;
             next = next->tree.right)
            PrettyInlineComment(f, next->tree.left->comment.comment, " ", "");
        c = next;
        FilePuts(f, ", ");
    }
    FilePuts(f, ")");
}

/*
 * Print the subscripts of an array, hash, or function type, as
 * FilePutSubscriptType does, with the comments of function types'
 * parameters
 */
static void
PrettySubscripts(Value f, Type *t)
{
    ExprPtr dims;

    switch (t->base.tag) {
    case type_func:
        PrettyTypeArgs(f, t->func.args);
        PrettySubscripts(f, t->func.ret);
        break;
    case type_array:
        FilePuts(f, "[");
        for (dims = t->array.dimensions; dims; dims = dims->tree.right) {
            if (dims->tree.left)
                PrettyExpr(f, dims->tree.left, -1, 0, False);
            else if (t->array.resizable)
                FilePuts(f, "...");
            else
                FilePuts(f, "*");
            if (dims->tree.right)
                FilePuts(f, ", ");
        }
        FilePuts(f, "]");
        PrettySubscripts(f, t->array.type);
        break;
    case type_hash:
        FilePuts(f, "[");
        PrettyTypeLine(f, t->hash.keyType);
        FilePuts(f, "]");
        PrettySubscripts(f, t->hash.type);
        break;
    default:
        break;
    }
}

/* Print a type on one line, with the comments of function types' parameters */
static void
PrettyTypeLine(Value f, Type *t)
{
    if (t && (t->base.tag == type_func || t->base.tag == type_array || t->base.tag == type_hash)) {
        FilePutBaseType(f, t, False);
        PrettySubscripts(f, t);
    } else if (t && t->base.tag == type_ref) {
        FilePuts(f, t->ref.pointer ? "*" : "&");
        PrettyTypeLine(f, t->ref.ref);
    } else
        FilePutType(f, t, False);
}

/*
 * Print a type starting at column col, with after more columns
 * following it on its line. When comments is set, a struct or union
 * prints on one line only when its members come from one declaration,
 * as 'struct { int a, b; }', none has comments, and the line fits as
 * code, and otherwise one declaration per line, aligned as
 * PrettyMembers does; an enum prints on one line when none of its
 * values has comments and the line fits, and otherwise as
 * PrettyEnumValues does. Other types, and these in error messages,
 * print on one line
 */
static void
PrettyType(Value f, Type *t, int level, Bool comments, ptrdiff_t col, ptrdiff_t after)
{
    ENTER();
    StructType   *st;
    PrettyMember *m;
    ptrdiff_t     i, j, k, n;
    Bool          enumeration;
    Value         one;

    /* An array, hash, or function of a struct or union lays out the struct */
    if (comments && t
        && (t->base.tag == type_array || t->base.tag == type_hash || t->base.tag == type_func)) {
        Type *base = t;

        for (;;) {
            if (base->base.tag == type_array)
                base = base->array.type;
            else if (base->base.tag == type_hash)
                base = base->hash.type;
            else if (base->base.tag == type_func)
                base = base->func.ret;
            else
                break;
        }
        if (base->base.tag == type_struct || base->base.tag == type_union) {
            Value subscripts = FileStringWrite();

            PrettySubscripts(subscripts, t);
            subscripts = FileStringString(subscripts);
            PrettyType(f, base, level, comments, col, PrettyTextColumns(subscripts) + after);
            FilePutsc(f, StringChars(&subscripts->string), subscripts->string.length);
            EXIT();
            return;
        }
    }
    /* Types joined with '+', as 'widget_t + struct { ... }', lay out each part */
    if (comments && t && (t->base.tag == type_struct || t->base.tag == type_union)
        && t->structs.left && t->structs.right) {
        Value left = FileStringWrite();

        PrettyType(left, t->structs.left, level, comments, col, 0);
        left = FileStringString(left);
        FilePutsc(f, StringChars(&left->string), left->string.length);
        FilePuts(f, " + ");
        if (PrettyMultiline(left))
            col = PrettyTextColumns(left);
        else
            col += PrettyTextColumns(left);
        PrettyType(f, t->structs.right, level, comments, col + 3, after);
        EXIT();
        return;
    }
    if (!comments) {
        FilePutType(f, t, False);
        EXIT();
        return;
    }
    if (!t || (t->base.tag != type_struct && t->base.tag != type_union)
        || (t->structs.left && t->structs.right)) {
        PrettyTypeLine(f, t);
        EXIT();
        return;
    }
    st = t->structs.structs;
    one = FileStringWrite();
    FilePutType(one, t, False);
    one = FileStringString(one);
    enumeration = t->structs.enumeration;
    if (!st->comments && (enumeration || PrettyOneDeclaration(st))
        && PrettyCodeFits((ptrdiff_t)level * 4, col + PrettyTextColumns(one) + after)) {
        FilePutsc(f, StringChars(&one->string), one->string.length);
        EXIT();
        return;
    }
    if (enumeration) {
        FilePuts(f, "enum {\n");
        PrettyEnumValues(f, st, level + 1);
        PrettyIndent(f, 0, level);
        FilePuts(f, "}");
        EXIT();
        return;
    }
    m = AllocateTemp((size_t)st->nelements * sizeof(PrettyMember));
    for (i = 0, n = 0; i < st->nelements; i = j, n++) {
        Value rest = FileStringWrite();
        Value type = FileStringWrite();
        Value text;
        Expr *group;

        /* Members declared together print together, as 'int x, y;' */
        for (j = i + 1; j < st->nelements && StructTypeGrouped(st)[j]; j++)
            ;
        group = PrettyGroupComments(st, i, j);
        m[n].comments = PrettyPastType(group);
        m[n].text = 0;
        m[n].stat = 0;
        m[n].decl = 0;
        m[n].blank = False;
        m[n].spaced = False;
        m[n].one_line = False;
        for (k = i; k < j; k++) {
            if (k > i)
                FilePuts(rest, ", ");
            FilePuts(rest, AtomName(StructTypeAtoms(st)[k]));
        }
        FileOutput(rest, ';');
        m[n].rest = FileStringString(rest);
        PrettyType(type, BoxTypesElements(st->types)[i], level + 1, True,
                   (ptrdiff_t)(level + 1) * 4, 1 + PrettyTextColumns(m[n].rest));
        text = FileStringWrite();
        PrettyTypeText(text, FileStringString(type), group);
        m[n].type = FileStringString(text);
    }
    if (t->base.tag == type_struct)
        FilePuts(f, "struct {\n");
    else
        FilePuts(f, "union {\n");
    PrettyMembers(f, m, n, level + 1, True);
    PrettyIndent(f, 0, level);
    FilePuts(f, "}");
    EXIT();
}

/*
 * Comment layout. A comment's text is split into lines, each with its
 * indentation in columns (tabs stop every eight) and its text, after
 * removing the comment marks: the opening and closing marks of a
 * block comment, with any extra '*'s next to them, the '*' starting
 * each later line when every non-blank later line starts with one,
 * and the '#' (or '#!') starting each line of a '#' comment. Text on
 * the opening line starts at the indentation of the others. Builtin
 * doc strings are plain text without marks.
 *
 * The lines are then kept as written, each indented from the
 * smallest indentation, the base, as far as it was.
 */

typedef struct _prettyLine {
    ptrdiff_t   indent; /* -1 for a blank line */
    const char *text;
    ptrdiff_t   len; /* bytes */
} PrettyLine;

/* Columns taken by len bytes of UTF-8 text */
static ptrdiff_t
PrettyColumns(const char *text, ptrdiff_t len)
{
    ptrdiff_t cols = 0, i;

    for (i = 0; i < len; i++)
        if ((text[i] & 0xc0) != 0x80)
            cols++;
    return cols;
}

/*
 * Measure a line of comment text from start to end, which begins at
 * column col; star replaces a leading '*' with a space
 */
static void
PrettyMeasureLine(PrettyLine *line, const char *start, const char *end, ptrdiff_t col, Bool star)
{
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        end--;
    for (; start < end; start++) {
        if (*start == ' ')
            col++;
        else if (*start == '\t')
            col = (col / 8 + 1) * 8;
        else if (*start == '*' && star) {
            col++;
            star = False;
        } else
            break;
    }
    if (start == end) {
        line->indent = -1;
        line->text = start;
        line->len = 0;
    } else {
        line->indent = col;
        line->text = start;
        line->len = end - start;
    }
}

/* Split a comment into lines without its marks; return the line count */
static ptrdiff_t
PrettyCommentLines(Value text, PrettyLine **linesp)
{
    const char *s = StringChars(&text->string);
    ptrdiff_t   len = text->string.length;
    const char *end = s + len, *p, *e;
    ptrdiff_t   n, i, first, last, base;
    PrettyLine *lines;
    Bool        block, hash, star;
    const char *start0;

    while (s < end && (*s == ' ' || *s == '\t'))
        s++;
    block = end - s >= 4 && s[0] == '/' && s[1] == '*' && end[-2] == '*' && end[-1] == '/';
    hash = !block && s < end && *s == '#';
    if (!block && !hash)
        s = StringChars(&text->string);

    n = 1;
    for (p = s; p < end; p++)
        if (*p == '\n')
            n++;
    lines = AllocateTemp((size_t)n * sizeof(PrettyLine));

    /* The opening mark, with any '*'s after it, and the closing mark */
    start0 = s;
    if (block) {
        start0 = s + 2;
        while (start0 < end - 2 && *start0 == '*')
            start0++;
        end -= 2;
        while (end > start0 && end[-1] == '*')
            end--;
    }

    /* A '*' starts each later line when every non-blank one has one */
    star = False;
    if (block) {
        Bool any = False;

        star = True;
        for (p = start0; p < end && *p != '\n'; p++)
            ;
        while (p < end) {
            for (e = ++p; e < end && *e != '\n'; e++)
                ;
            while (p < e && (*p == ' ' || *p == '\t'))
                p++;
            if (p < e) {
                any = True;
                if (*p != '*')
                    star = False;
            }
            p = e;
        }
        star = star && any;
    }

    for (i = 0, p = start0; i < n; i++) {
        for (e = p; e < end && *e != '\n'; e++)
            ;
        if (hash) {
            const char *h = p;

            while (h < e && *h != '#')
                h++;
            if (h < e)
                h++;
            if (i == 0 && h < e && *h == '!')
                h++;
            PrettyMeasureLine(&lines[i], h, e, PrettyColumns(p, h - p), False);
        } else
            PrettyMeasureLine(&lines[i], p, e, 0, star && i > 0);
        p = e + 1;
        if (e >= end) {
            n = i + 1;
            break;
        }
    }

    /* The opening line's text starts at the others' indentation */
    if (n > 1 && lines[0].indent >= 0) {
        base = -1;
        for (i = 1; i < n; i++)
            if (lines[i].indent >= 0 && (base < 0 || lines[i].indent < base))
                base = lines[i].indent;
        if (base >= 0)
            lines[0].indent = base;
    }

    /* Blank lines around a block comment's text go; each '#' line was written */
    first = 0;
    last = n;
    if (!hash) {
        while (first < n && lines[first].indent < 0)
            first++;
        while (last > first && lines[last - 1].indent < 0)
            last--;
    }
    *linesp = lines + first;
    return last - first;
}

static void
PrettySpaces(Value out, ptrdiff_t n)
{
    while (n-- > 0)
        FileOutput(out, ' ');
}

/*
 * Write text as written, expanding its tabs from column col, where it
 * started, so it stays aligned wherever it's printed
 */
static void
PrettyPutExpanded(Value out, const char *text, ptrdiff_t len, ptrdiff_t col)
{
    ptrdiff_t i;

    for (i = 0; i < len; i++) {
        if (text[i] == '\t') {
            ptrdiff_t next = (col / 8 + 1) * 8;

            PrettySpaces(out, next - col);
            col = next;
        } else {
            FileOutput(out, text[i]);
            if ((text[i] & 0xc0) != 0x80)
                col++;
        }
    }
}

/* Lay out the comment text, each line indented from the base as far as it was */
static Value
PrettyLayoutComment(PrettyLine *lines, ptrdiff_t n)
{
    ENTER();
    Value     out = FileStringWrite();
    ptrdiff_t base = -1, i;

    for (i = 0; i < n; i++)
        if (lines[i].indent >= 0 && (base < 0 || lines[i].indent < base))
            base = lines[i].indent;
    for (i = 0; i < n; i++) {
        if (lines[i].indent >= 0) {
            PrettySpaces(out, lines[i].indent - base);
            PrettyPutExpanded(out, lines[i].text, lines[i].len, lines[i].indent);
        }
        FileOutput(out, '\n');
    }
    RETURN(FileStringString(out));
}

/*
 * Print a comment as a one-line comment if its text lays out as one
 * line fitting in width columns; return whether it did
 */
static Bool
PrettyCommentOneLine(Value f, Value text, ptrdiff_t width)
{
    ENTER();
    PrettyLine *lines;
    ptrdiff_t   n = PrettyCommentLines(text, &lines);
    Value       laid;
    const char *s, *e, *end;
    Bool        fits = False;

    if (n == 0) {
        if (width >= 5) {
            FilePuts(f, "/* */");
            fits = True;
        }
    } else {
        laid = PrettyLayoutComment(lines, n);
        s = StringChars(&laid->string);
        end = s + laid->string.length;
        for (e = s; e < end && *e != '\n'; e++)
            ;
        if (e + 1 >= end && 6 + PrettyColumns(s, e - s) <= width) {
            FilePuts(f, "/* ");
            FilePutsc(f, s, e - s);
            FilePuts(f, " */");
            fits = True;
        }
    }
    EXIT();
    return fits;
}

/* Whether a comment's text was written with '#' marks */
static Bool
PrettyHashText(Value text)
{
    const char *s = StringChars(&text->string);
    const char *end = s + text->string.length;

    while (s < end && (*s == ' ' || *s == '\t'))
        s++;
    return s < end && *s == '#';
}

/*
 * Print a comment at level, starting at the indentation and ending
 * after its closing mark: one written with '#' marks as lines starting
 * with '# ', as it was written, else on one line when its text is one
 * line, and otherwise as a block with ' * ' before each line. A '#'
 * comment ends its line, so this prints only comments with lines of
 * their own
 */
static void
PrettyComment(Value f, Value text, int level)
{
    ENTER();
    PrettyLine *lines;
    ptrdiff_t   n;
    Value       laid;
    const char *s, *e, *end;
    Bool        hash = PrettyHashText(text);

    PrettyIndent(f, 0, level);
    if (!hash && PrettyCommentOneLine(f, text, PTRDIFF_MAX)) {
        EXIT();
        return;
    }
    n = PrettyCommentLines(text, &lines);
    laid = PrettyLayoutComment(lines, n);
    if (hash) {
        s = StringChars(&laid->string);
        end = s + laid->string.length;
        for (Bool first = True; s < end || first; s = e + 1, first = False) {
            for (e = s; e < end && *e != '\n'; e++)
                ;
            if (!first)
                PrettyIndent(f, 0, level);
            FileOutput(f, '#');
            if (e > s) {
                FileOutput(f, ' ');
                FilePutsc(f, s, e - s);
            }
            if (e + 1 < end)
                FileOutput(f, '\n');
        }
        EXIT();
        return;
    }
    FilePuts(f, "/*\n");
    s = StringChars(&laid->string);
    end = s + laid->string.length;
    for (; s < end; s = e + 1) {
        for (e = s; e < end && *e != '\n'; e++)
            ;
        PrettyIndent(f, 0, level);
        if (e > s) {
            FilePuts(f, " * ");
            FilePutsc(f, s, e - s);
        } else
            FilePuts(f, " *");
        FileOutput(f, '\n');
    }
    PrettyIndent(f, 0, level);
    FilePuts(f, " */");
    EXIT();
}

static void
PrettyBody(Value f, CodePtr code, int level, Bool nest, Bool attach, ptrdiff_t col)
{
    /*
     * A definition's parameters, starting at column col, break as a
     * call's arguments do; the brace of a catch or lambda whose
     * parameters break goes on a line of its own, as a wrapped
     * header's does
     */
    if (col >= 0) {
        StackPointer mark = MemMark();
        Value        args = FileStringWrite();

        PrettyWrapArgs(args, code->base.args, level, col, attach ? 2 : 0);
        args = FileStringString(args);
        if (PrettyMultiline(args))
            attach = False;
        FilePutsc(f, StringChars(&args->string), args->string.length);
        MemRelease(mark);
    } else
        PrettyArgs(f, code->base.args, level, True);
    if (code->base.doc != Void) {
        FilePuts(f, "\n");
        PrettyComment(f, code->base.doc, level + 1);
        attach = False;
    }
    if (!code->base.builtin && PrettyHasDoc(code->func.code)) {
        (void)PrettyDocComments(f, code->func.code, level);
        attach = False;
    }
    if (nest) {
        if (code->base.builtin) {
            FilePuts(f, " <builtin>");
        } else {
            /* A function's brace starts its own line; a catch's follows its arguments */
            if (attach)
                FilePuts(f, " {\n");
            else {
                FilePuts(f, "\n");
                PrettyIndent(f, 0, level);
                FilePuts(f, "{\n");
            }
            PrettyBlock(f, code->func.code, level + 1, nest, attach);
            PrettyIndent(f, 0, level);
            FilePuts(f, "}");
        }
    } else
        FilePuts(f, ";");
}

/* Print a function's parameters and ' = value', for a body returning one value */
static void
PrettyShortCode(Value f, CodePtr code, Expr *value, int level, Bool nest)
{
    PrettyArgs(f, code->base.args, level, True);
    FilePuts(f, " = ");
    PrettyExpr(f, value, PrettyPrecedence(ASSIGN), level, nest);
}

/*
 * Print a function definition's publication, class, and return type,
 * with the comments its code holds among them when printing nested
 */
static void
PrettyCodeType(Value f, CodePtr code, Class class, Publish publish, Bool nest)
{
    ENTER();
    Value type = FileStringWrite();

    if (nest) {
        FilePrintf(type, "%p%k", publish, class);
        PrettyTypeLine(type, code->base.type);
        PrettyTypeText(f, FileStringString(type),
                       code->base.name ? code->base.name->base.comments : 0);
    } else
        FilePrintf(f, "%p%k%T", publish, class, code->base.type);
    EXIT();
}

/*
 * Whether comments before a definition's name end their lines, so its
 * name starts a line under its return type
 */
static Bool
PrettyCodeTypeBreaks(CodePtr code)
{
    Expr *c;

    for (c = code->base.name ? code->base.name->base.comments : 0; c; c = c->tree.right) {
        CommentPtr comment = c->tree.left->comment.comment;

        if (comment->place == comment_before && comment->ends_line)
            return True;
    }
    return False;
}

/*
 * Print the comments before a definition's name that end their lines,
 * after its return type, or on a line of their own between the two
 */
static void
PrettyCodeTypeEnds(Value f, CodePtr code, int level)
{
    Expr *c;

    for (c = code->base.name ? code->base.name->base.comments : 0; c; c = c->tree.right) {
        CommentPtr comment = c->tree.left->comment.comment;
        Value      one;

        if (comment->place != comment_before || !comment->ends_line)
            continue;
        one = FileStringWrite();
        if (!PrettyCommentOneLine(one, comment->text, PRETTY_WIDTH))
            continue;
        if (comment->starts_line) {
            FileOutput(f, '\n');
            PrettyIndent(f, 0, level);
        } else
            FileOutput(f, ' ');
        one = FileStringString(one);
        FilePutsc(f, StringChars(&one->string), one->string.length);
        comment->printed = True;
    }
}

/*
 * Print a function's name, with the comments its code holds around the
 * name when printing nested, apart from those ending their lines,
 * returning the columns they take
 */
static ptrdiff_t
PrettyCodeName(Value f, CodePtr code, Atom name, Bool nest)
{
    ENTER();
    Value     text = FileStringWrite();
    Expr     *e = code->base.name, *c;
    Bool      was = prettyMarking;
    ptrdiff_t cols;

    prettyMarking = False;
    if (nest && e)
        for (c = e->base.comments; c; c = c->tree.right)
            if (c->tree.left->comment.comment->place == comment_before
                && !c->tree.left->comment.comment->ends_line)
                PrettyCommentNote(text, c->tree.left->comment.comment);
    FilePrintf(text, "%A", name);
    if (nest && e)
        PrettyCommentList(text, e->base.comments, comment_after);
    prettyMarking = was;
    text = FileStringString(text);
    FilePutsc(f, StringChars(&text->string), text->string.length);
    cols = PrettyTextColumns(text);
    EXIT();
    return cols;
}

/*
 * The value a function's body returns when the body is that return
 * alone, without comments or a doc string, and the function then fits
 * on one line as 'type name(args) = value;', or for a lambda as
 * 'func(args) = value', whether written that way or as a block; 0
 * otherwise, as when a parameter starts a line in the source, and the
 * function prints with a block. A definition's return type, type,
 * starts the line, or is 0 when the type takes a line of its own
 */
static Expr *
PrettyShortBody(CodePtr code, Atom name, Value type, int level)
{
    ENTER();
    Expr     *body, *value;
    Value     text;
    Bool      was = prettyMarking;
    ptrdiff_t start = 0, end;
    ArgType  *arg;

    if (code->base.builtin || code->base.doc != Void || profiling) {
        EXIT();
        return 0;
    }
    for (arg = code->base.args; arg; arg = arg->next)
        if (arg != code->base.args && arg->starts_line) {
            EXIT();
            return 0;
        }
    body = code->func.code;
    if (!body || !body->tree.left || !body->tree.right || body->tree.right->tree.left
        || body->tree.left->base.tag != RETURNTOK || !body->tree.left->tree.right) {
        EXIT();
        return 0;
    }
    value = body->tree.left->tree.right;
    text = FileStringWrite();
    prettyMarking = False;
    if (type) {
        FilePutsc(text, StringChars(&type->string), type->string.length);
        FileOutput(text, ' ');
    }
    if (name)
        PrettyCodeName(text, code, name, True);
    else
        FilePrintf(text, "%tfunc", code->base.type);
    PrettyShortCode(text, code, value, level, True);
    prettyMarking = was;
    if (name) {
        start = (ptrdiff_t)level * 4;
        FileOutput(text, ';');
    }
    text = FileStringString(text);
    end = start + PrettyTextColumns(text);
    if (PrettyMultiline(text) || !PrettyCodeFits(start, end))
        value = 0;
    EXIT();
    return value;
}

/*
 * Print a definition that fits on one line as 'type name(args) =
 * value;', its return type to type and the rest of its line to rest,
 * returning whether it does; a comment ending its line after the type
 * keeps the type on a line of its own, so the definition doesn't
 */
static Bool
PrettyShortDefinition(Value type, Value rest, CodePtr code, Atom name, Class class, Publish publish,
                      int level)
{
    ENTER();
    Value text = FileStringWrite();
    Bool  was = prettyMarking;
    Expr *value;

    if (code->base.builtin || PrettyCodeTypeBreaks(code)) {
        EXIT();
        return False;
    }
    PrettyCodeType(text, code, class, publish, True);
    text = FileStringString(text);
    value = PrettyShortBody(code, name, text, level);
    if (!value) {
        EXIT();
        return False;
    }
    FilePutsc(type, StringChars(&text->string), text->string.length);
    prettyMarking = False;
    PrettyCodeName(rest, code, name, True);
    PrettyShortCode(rest, code, value, level, True);
    FileOutput(rest, ';');
    prettyMarking = was;
    EXIT();
    return True;
}

/* Print a definition on one line when it fits there, returning whether it does */
static Bool
PrettyShortLine(Value f, CodePtr code, Atom name, Class class, Publish publish, int level)
{
    ENTER();
    Value type = FileStringWrite();
    Value rest = FileStringWrite();

    if (!PrettyShortDefinition(type, rest, code, name, class, publish, level)) {
        EXIT();
        return False;
    }
    type = FileStringString(type);
    rest = FileStringString(rest);
    FilePutsc(f, StringChars(&type->string), type->string.length);
    FileOutput(f, ' ');
    FilePutsc(f, StringChars(&rest->string), rest->string.length);
    EXIT();
    return True;
}

void
PrettyCode(Value f, CodePtr code, Atom name, Class class, Publish publish, int level, Bool nest)
{
    ptrdiff_t args = -1, name_cols = 0;
    Expr     *value = 0;

    if (name && nest && PrettyShortLine(f, code, name, class, publish, level))
        return;
    /* A lambda, or a definition whose type ends its line, can still print as '= value' */
    if (nest && (!name || PrettyCodeTypeBreaks(code)))
        value = PrettyShortBody(code, name, 0, level);
    if (name) {
        PrettyCodeType(f, code, class, publish, nest);
        /* A definition's name starts a line, as clang-format puts it */
        if (nest && !code->base.builtin) {
            PrettyCodeTypeEnds(f, code, level);
            FileOutput(f, '\n');
            PrettyIndent(f, 0, level);
        } else
            FileOutput(f, ' ');
        name_cols = PrettyCodeName(f, code, name, nest);
    } else {
        FilePrintf(f, "%tfunc", code->base.type);
        /* Summarized in an expression, a function shows its arguments without a ';' */
        if (!nest) {
            PrettyArgs(f, code->base.args, level, False);
            return;
        }
    }
    if (value) {
        PrettyShortCode(f, code, value, level, nest);
        if (name)
            FilePuts(f, ";");
        return;
    }
    /* A definition's parameters break after its name */
    if (name && nest && !code->base.builtin && !profiling)
        args = (ptrdiff_t)level * 4 + name_cols;
    /* A lambda printed from column 0, as laying it out does, knows where its parameters go */
    if (!name && nest && !code->base.builtin && !profiling && level == 0) {
        Value prefix = FileStringWrite();

        FilePrintf(prefix, "%tfunc", code->base.type);
        args = PrettyTextColumns(FileStringString(prefix));
    }
    /* A lambda's brace follows its arguments */
    PrettyBody(f, code, level, nest, !name, args);
    if (!code->base.builtin && nest && profiling) {
        double_digit sub = 0, self = 0;
        if (code->func.body.obj) {
            sub += code->func.body.obj->sub_ticks;
            self += code->func.body.obj->ticks;
        }
        if (code->func.staticInit.obj) {
            sub += code->func.staticInit.obj->sub_ticks;
            self += code->func.staticInit.obj->ticks;
        }
        FilePuts(f, "\n---------------------\n");
        PrettyProfNum(f, sub + self, 1);
        FilePuts(f, " ");
        PrettyProfNum(f, self, 1);
        if (name)
            FilePrintf(f, ": %A\n", name);
        else
            FilePrintf(f, ": %tfunc\n", code->base.type);
    }
}

void
PrettyStat(Value f, Expr *e, Bool nest)
{
    PrettyStatement(f, e, 1, 1, nest);
}

static void PrettyCommentedSymbol(Value f, Publish publish, SymbolPtr symbol, int level, Bool nest);

/*
 * Whether nothing has printed yet at the start of the output or of a
 * namespace's braces, where a declaration's leading comments get no
 * blank line above them
 */
static Bool prettyFirstDeclaration = True;

/*
 * Whether printing full definitions, as the source wrote them, or a
 * summary, and in full, whether the last name printed was a function
 * definition or a namespace
 */
static Bool prettyFull, prettyLastBig;

/*
 * Whether a name prints as a function definition with a block or a
 * namespace
 */
static Bool
PrettySymbolBig(SymbolPtr symbol, Publish publish, int level)
{
    ENTER();
    Value v;
    Bool  big = False;

    switch (symbol->symbol.class) {
    case class_const:
    case class_global:
        v = BoxValueGet(symbol->global.value, 0);
        big = v && ValueIsFunc(v) && !v->func.code->base.builtin
            && !PrettyShortDefinition(FileStringWrite(), FileStringWrite(), v->func.code,
                                      symbol->symbol.name, class_undef, publish, level);
        break;
    case class_namespace:
        big = True;
        break;
    default:
        break;
    }
    EXIT();
    return big;
}

/*
 * Print a namespace's names, in the order they were declared: in a
 * summary, those not private, functions as declarations; in full, all
 * of them, with function bodies and namespaces' contents, a blank line
 * around each function definition or namespace as formatting puts it
 */
static void
PrettyNames(Value f, NamelistPtr names, int level)
{
    Bool big;

    if (!names)
        return;
    PrettyNames(f, names->next, level);
    if (!prettyFull) {
        if (names->publish != publish_private)
            PrettyCommentedSymbol(f, names->publish, names->symbol, level, False);
        return;
    }
    big = PrettySymbolBig(names->symbol, names->publish, level);
    if (!prettyFirstDeclaration && (big || prettyLastBig)) {
        FileOutput(f, '\n');
        /* The blank line serves the name's leading comments too */
        prettyFirstDeclaration = True;
    }
    prettyLastBig = big;
    PrettyCommentedSymbol(f, names->publish, names->symbol, level, True);
}

static void
PrettyNamespace(Value f, NamespacePtr namespace, int level)
{
    Bool last = prettyLastBig;

    prettyFirstDeclaration = True;
    prettyLastBig = False;
    PrettyNames(f, namespace->names, level);
    prettyFirstDeclaration = False;
    prettyLastBig = last;
}

static void
PrettySymbol(Value f, Publish publish, SymbolPtr symbol, int level, Bool nest)
{
    Value v;

    PrettyIndent(f, 0, level);
    switch (symbol->symbol.class) {
    case class_const:
    case class_global:
        v = BoxValueGet(symbol->global.value, 0);
        if (v && ValueIsFunc(v)) {
            PrettyCode(f, v->func.code, symbol->symbol.name, class_undef, publish, level, nest);
        } else {
            /* In full, as the source would declare it, without 'global' */
            Type *t = symbol->symbol.type;

            FilePrintf(f, "%p%k", publish,
                       prettyFull && symbol->symbol.class == class_global ? class_undef
                                                                          : symbol->symbol.class);
            /* With the comments of function types' parameters */
            if (nest && t
                && (t->base.tag == type_func || t->base.tag == type_array
                    || t->base.tag == type_hash || t->base.tag == type_ref)) {
                PrettyTypeLine(f, t);
                FileOutput(f, ' ');
            } else
                FilePrintf(f, "%t", t);
            FilePrintf(f, "%A", symbol->symbol.name);
            /* An uninitialized global prints without an initializer */
            if (v)
                FilePrintf(f, " = %v", v);
            FilePuts(f, ";");
        }
        FilePuts(f, "\n");
        break;
    case class_namespace:
        /* A namespace declaration needs its body, even inside a summary */
        FilePrintf(f, "%p%C %A {\n", publish, symbol->symbol.class, symbol->symbol.name);
        PrettyNamespace(f, symbol->namespace.namespace, level + 1);
        PrettyIndent(f, 0, level);
        FilePuts(f, "}\n");
        break;
    case class_exception: {
        Value prefix = FileStringWrite();
        Bool  doc = symbol->exception.doc != Void || PrettyHasDoc(symbol->symbol.comments);

        FilePrintf(prefix, "%p%C %A", publish, symbol->symbol.class, symbol->symbol.name);
        prefix = FileStringString(prefix);
        FilePutsc(f, StringChars(&prefix->string), prefix->string.length);
        if (nest && !profiling)
            PrettyWrapArgs(f, symbol->symbol.type->func.args, level,
                           (ptrdiff_t)level * 4 + PrettyTextColumns(prefix), doc ? 0 : 1);
        else
            PrettyArgs(f, symbol->symbol.type->func.args, level, True);
        if (symbol->exception.doc != Void) {
            FilePuts(f, "\n");
            PrettyComment(f, symbol->exception.doc, level + 1);
        }
        PrettyDocSemicolon(f, symbol->symbol.comments, level);
        FilePuts(f, "\n");
        break;
    }
    default: {
        Value prefix = FileStringWrite();

        FilePrintf(prefix, "%p%k", publish, symbol->symbol.class);
        prefix = FileStringString(prefix);
        FilePutsc(f, StringChars(&prefix->string), prefix->string.length);
        PrettyType(f, symbol->symbol.type, level, True,
                   (ptrdiff_t)level * 4 + PrettyTextColumns(prefix),
                   2
                       + StringLength(AtomName(symbol->symbol.name),
                                      (ptrdiff_t)strlen(AtomName(symbol->symbol.name))));
        FilePrintf(f, " %A;\n", symbol->symbol.name);
        break;
    }
    }
}

/*
 * Print a symbol's declaration with the comments it had: the leading
 * ones before it and the trailing ones after it
 */
static void
PrettyCommentedSymbol(Value f, Publish publish, SymbolPtr symbol, int level, Bool nest)
{
    ENTER();
    Expr *c;
    Value printed;

    if (!symbol) {
        EXIT();
        return;
    }
    if (profiling)
        FilePuts(f, "     total(ms)   self(ms)\n");
    c = symbol->symbol.comments;
    /* Leading comments have a blank line above them, as in function bodies */
    if (!prettyFirstDeclaration && PrettyHasLeading(c))
        FileOutput(f, '\n');
    prettyFirstDeclaration = False;
    /* Blank lines from the source don't print around one declaration */
    for (; c && c->tree.left && c->tree.left->comment.comment->place != comment_trailing;
         c = c->tree.right) {
        /* An exception prints its doc comments */
        if (PrettyBlankEntry(c) || c->tree.left->comment.comment->place == comment_doc)
            continue;
        PrettyComment(f, c->tree.left->comment.comment->text, level);
        FileOutput(f, '\n');
    }
    if (!c || !c->tree.left) {
        PrettySymbol(f, publish, symbol, level, nest);
        EXIT();
        return;
    }
    printed = FileStringWrite();
    PrettySymbol(printed, publish, symbol, level, nest);
    PrettyTrailingComments(f, FileStringString(printed), c, level, True, 0);
    EXIT();
}

/*
 * Print a name, or with none every public name, as a summary or in full
 * (see PrettyNames)
 */
void
PrettyPrint(Value f, Publish publish, SymbolPtr name, Bool full)
{
    Bool was = prettyFull;

    prettyFull = full;
    prettyFirstDeclaration = True;
    if (!name)
        PrettyNamespace(f, TopNamespace, 0);
    else
        PrettyCommentedSymbol(f, publish, name, 0, True);
    prettyFull = was;
}

static Expr *PrettySummary(Expr *statements, Bool top);

/*
 * A statement as a summary shows it, or 0 when it shows none: a
 * declaration, a function definition as a declaration without its
 * body, and a namespace with its summary; inside a namespace, only
 * those not private
 */
static Expr *
PrettySummaryStatement(Expr *e, Bool top)
{
    ENTER();
    Expr *decl;

    switch (e->base.tag) {
    case EXPR:
        decl = e->tree.left;
        if (decl->base.tag != VAR)
            RETURN(0);
        break;
    case TYPEDEF:
    case NAMESPACE:
        decl = e->tree.left;
        break;
    case FUNC:
    case EXCEPTION:
        decl = e;
        break;
    default:
        RETURN(0);
    }
    if (!top && decl->decl.publish == publish_private)
        RETURN(0);
    if (e->base.tag == NAMESPACE)
        RETURN(NewExprTree(NAMESPACE, e->tree.left, PrettySummary(e->tree.right, False)));
    if (e->base.tag == FUNC && e->decl.decl->init)
        RETURN(NewExprDecl(FUNC, NewDeclList(e->decl.decl->name, 0, 0), e->decl.class, e->decl.type,
                           e->decl.publish));
    RETURN(e);
}

/*
 * A statement list as a summary shows it: the statements
 * PrettySummaryStatement keeps, with the comments leading and trailing
 * them, and those after the last statement
 */
static Expr *
PrettySummary(Expr *statements, Bool top)
{
    ENTER();
    Expr *out = 0, **last = &out, *pending = 0, **pending_last = &pending, *s, *kept;
    Bool  trailing = False;

    for (s = statements; s && s->tree.left; s = s->tree.right) {
        Expr *e = s->tree.left;

        if (e->base.tag == COMMENT_CONST) {
            if (e->comment.comment->place == comment_trailing) {
                if (trailing) {
                    *last = NewExprTree(OC, e, 0);
                    last = &(*last)->tree.right;
                }
            } else {
                *pending_last = NewExprTree(OC, e, 0);
                pending_last = &(*pending_last)->tree.right;
            }
            continue;
        }
        kept = PrettySummaryStatement(e, top);
        trailing = kept != 0;
        if (kept) {
            *last = pending;
            if (pending)
                last = pending_last;
            *last = NewExprTree(OC, kept, 0);
            last = &(*last)->tree.right;
        }
        pending = 0;
        pending_last = &pending;
    }
    *last = pending;
    if (pending)
        last = pending_last;
    *last = NewExprTree(OC, 0, 0);
    RETURN(out);
}

/*
 * The top-level commands of the input being formatted, with their
 * comments, as a statement list; prettyFormatLast is the empty entry
 * ending it
 */
static Expr        *prettyFormatCommands;
static Expr       **prettyFormatLast;
static ReferencePtr prettyFormatCommandsReference;

/* Add a command of the input being formatted, with its comments */
void
PrettyFormatAppend(Expr *command, LexLocation location)
{
    ENTER();
    CommentPtr leading, trailing;

    if (!prettyFormatCommandsReference) {
        prettyFormatCommandsReference = NewReference((void **)&prettyFormatCommands);
        MemAddRoot(prettyFormatCommandsReference);
    }
    if (!prettyFormatCommands) {
        prettyFormatCommands = NewExprTree(OC, 0, 0);
        prettyFormatLast = &prettyFormatCommands;
    }
    /* reset counts each command as failing until it runs; this one won't run */
    lastThreadError = False;
    CommentSplitStatement(command, location, &leading, &trailing);
    *prettyFormatLast = CommentList(
        leading, NewExprTree(OC, command, CommentList(trailing, NewExprTree(OC, 0, 0))));
    while ((*prettyFormatLast)->tree.left)
        prettyFormatLast = &(*prettyFormatLast)->tree.right;
    EXIT();
}

/* Whether a command runs while formatting: those that load code do */
Bool
PrettyFormatRuns(Atom name)
{
    static const char * const runs[] = { "autoload", "autoimport", "reimport", "library", "load" };
    size_t                    i;

    for (i = 0; i < sizeof(runs) / sizeof(runs[0]); i++)
        if (!strcmp(AtomName(name), runs[i]))
            return True;
    return False;
}

/*
 * Print the formatted input, which has ended: the comments of its file
 * record before its first line of code, its top-level commands as a
 * statement list, and the record's comments after its code. '#'
 * comments in the record keep their marks as written. In full, every
 * command prints as written; as a summary, only what PrettySummary
 * keeps
 */
void
PrettyFormat(Value f, CommentPtr record, int first_line, Bool full)
{
    ENTER();
    CommentPtr comment;
    Bool       any = False;
    Expr      *s, *last, *statements = prettyFormatCommands;

    if (!statements)
        statements = NewExprTree(OC, 0, 0);
    if (!full)
        statements = PrettySummary(statements, True);

    for (comment = record; comment; comment = comment->next) {
        if (first_line && comment->first_line >= first_line)
            continue;
        if (any)
            FileOutput(f, '\n');
        if (comment->style == comment_block)
            PrettyComment(f, comment->text, 0);
        else
            FilePutsc(f, StringChars(&comment->text->string), comment->text->string.length);
        FileOutput(f, '\n');
        any = True;
    }
    /*
     * Of the first statement's leading comments, only the last, ending
     * just above it, leads it when the output is read again; the others
     * print as the file's
     */
    for (s = statements, last = 0; s && s->tree.left && s->tree.left->base.tag == COMMENT_CONST;
         s = s->tree.right)
        if (!PrettyBlankEntry(s))
            last = s;
    if (s && s->tree.left && last) {
        for (s = statements; s != last; s = s->tree.right) {
            if (PrettyBlankEntry(s))
                continue;
            if (any)
                FileOutput(f, '\n');
            PrettyComment(f, s->tree.left->comment.comment->text, 0);
            FileOutput(f, '\n');
            any = True;
        }
        statements = last;
    }
    if (statements && statements->tree.left) {
        if (any)
            FileOutput(f, '\n');
        PrettyBlock(f, statements, 0, True, False);
        any = True;
    }
    for (comment = record; comment; comment = comment->next) {
        if (!first_line || comment->first_line < first_line)
            continue;
        if (any)
            FileOutput(f, '\n');
        PrettyComment(f, comment->text, 0);
        FileOutput(f, '\n');
        any = True;
    }
    prettyFormatCommands = 0;
    prettyFormatLast = 0;
    EXIT();
}
