/*
 * Copyright © 2026 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * Comments, for the pretty-printer: the lexer records them and places
 * them by position, and the parser gives them to the constructs that
 * print them
 */

#include "nickle.h"
#include "gram.h"
#include "ref.h"

static void
CommentMark(void *object)
{
    Comment *comment = object;

    MemReference(comment->next);
    MemReference(comment->text);
}

static DataType CommentType = {
    .Mark = CommentMark,
    .Free = 0,
    .name = "CommentType",
};

/* Placed comments, waiting for the parser */
static Comment     *commentPlaced;
static ReferencePtr commentPlacedReference;

/*
 * The line and kind of each token from commentTokensFirst through
 * commentTokensLast, those whose comments may still be placed, so
 * comments can tell which lines they share with code, and which tokens
 * close what they're in
 */
typedef struct _commentToken {
    int line;
    int token;
} CommentToken;

static CommentToken *commentTokens;
static LexToken      commentTokensFirst = 1, commentTokensLast;
static ptrdiff_t     commentTokensSize;

/* The location of the rule being reduced, which the nodes its action builds span */
static LexLocation   commentLocation;

/*
 * The file records of the inputs read, by name, for printing a whole
 * file; reading a file again replaces its record
 */
typedef struct _commentFileRecord {
    DataType                  *data;
    struct _commentFileRecord *next;
    Atom                       name;
    Comment                   *comments;
} CommentFileRecord;

static void
CommentFileRecordMark(void *object)
{
    CommentFileRecord *record = object;

    MemReference(record->next);
    MemReference(record->comments);
}

static DataType CommentFileRecordType = {
    .Mark = CommentFileRecordMark,
    .Free = 0,
    .name = "CommentFileRecordType",
};
static CommentFileRecord *commentFileRecords;
static ReferencePtr       commentFileRecordsReference;

/* Add a comment to the end of a list */
void
CommentAppend(Comment **list, Comment *comment)
{
    while (*list)
        list = &(*list)->next;
    comment->next = 0;
    *list = comment;
}

/* A new comment, not yet placed */
CommentPtr
NewComment(Value text, int first_line, int last_line, CommentStyle style, Bool fall_through)
{
    ENTER();
    Comment *comment = MemAllocate(&CommentType, sizeof(Comment));

    comment->next = 0;
    comment->text = text;
    comment->first_line = first_line;
    comment->last_line = last_line;
    comment->style = style;
    comment->place = comment_leading;
    comment->token = 0;
    comment->fall_through = fall_through;
    comment->keyword = 0;
    comment->starts_line = comment->ends_line = comment->printed = False;
    RETURN(comment);
}

/* Place a comment, leading or trailing a token, for the parser to take */
void
CommentAddPlaced(CommentPtr comment, CommentPlace place, LexToken token)
{
    comment->place = place;
    comment->token = token;
    CommentAppend(&commentPlaced, comment);
}

/*
 * Record the blank lines before something starting on line, after the
 * input's first token, as an entry with no text waiting for the next
 * token, which the printer turns into a blank line
 */
static void
CommentInputBlank(CommentInput *in, int line)
{
    Comment *comment;

    if (!in->tokens || line <= in->content_line + 1)
        return;
    comment = NewComment(0, in->content_line + 1, line - 1, comment_blank, False);
    CommentAppend(&in->pending, comment);
}

/*
 * Finish the text of a comment leaving an input's pending comments: a
 * run of '#' lines joined there is a string file until then
 */
static void
CommentInputText(Comment *comment)
{
    if (comment->text && ValueIsFile(comment->text))
        comment->text = FileStringString(comment->text);
}

/*
 * Place the comments waiting for an input's token, numbered token,
 * which starts on line, and count it: before its first token, they go
 * in the file record, except one ending on the line just above it that
 * doesn't start with '#!'
 */
void
CommentInputToken(CommentInput *in, LexToken token, int line)
{
    Comment *comment, *next;

    CommentInputBlank(in, line);
    in->content_line = line;
    comment = in->pending;
    in->pending = 0;
    for (; comment; comment = next) {
        next = comment->next;
        CommentInputText(comment);
        if (in->tokens == 0
            && (next || comment->last_line != line - 1 || comment->style == comment_shebang)) {
            comment->place = comment_file;
            CommentAppend(&in->file_comments, comment);
        } else
            CommentAddPlaced(comment, comment_leading, token);
    }
    if (in->tokens == 0)
        in->first_line = line;
    in->tokens++;
    in->token_line = line;
}

/*
 * Record a comment of an input, read from first_line through
 * last_line: one starting on the line of the last token, numbered
 * token, trails it, and others wait for the next token
 */
void
CommentInputRecord(CommentInput *in, Value text, int first_line, int last_line, CommentStyle style,
                   Bool fall_through, LexToken token)
{
    ENTER();
    Comment *comment, *last;

    if (style != comment_block) {
        /* Consecutive '#' lines make one comment */
        for (last = in->pending; last && last->next; last = last->next)
            ;
        if (last && last->style != comment_block && last->last_line == first_line - 1) {
            /* Joined in a string file, which CommentInputText finishes */
            if (!ValueIsFile(last->text)) {
                Value f = FileStringWrite();

                FilePutsc(f, StringChars(&last->text->string), last->text->string.length);
                last->text = f;
            }
            FileOutput(last->text, '\n');
            FilePutsc(last->text, StringChars(&text->string), text->string.length);
            last->last_line = first_line;
            last->fall_through = False;
            in->content_line = last_line;
            EXIT();
            return;
        }
    }
    CommentInputBlank(in, first_line);
    in->content_line = last_line;
    comment = NewComment(text, first_line, last_line, style, fall_through);
    if (in->tokens && first_line == in->token_line)
        CommentAddPlaced(comment, comment_trailing, token);
    else
        CommentAppend(&in->pending, comment);
    EXIT();
}

/*
 * An input has ended: the comments after its last token join its file
 * record, which is returned
 */
CommentPtr
CommentInputFinish(CommentInput *in)
{
    Comment *comment, *next;

    for (comment = in->pending; comment; comment = next) {
        next = comment->next;
        CommentInputText(comment);
        /* The file record keeps no blank lines */
        if (comment->style == comment_blank)
            continue;
        comment->place = comment_file;
        CommentAppend(&in->file_comments, comment);
    }
    in->pending = 0;
    return in->file_comments;
}

/*
 * Remove and return, in order, the placed comments of the tokens in
 * location: those leading any of them, and those trailing any of them
 * when trailing is set
 */
CommentPtr
CommentTake(LexLocation location, Bool trailing)
{
    Comment  *taken = 0;
    Comment **prev, *comment;

    for (prev = &commentPlaced; (comment = *prev);) {
        if (location.first_token <= comment->token && comment->token <= location.last_token
            && (trailing || comment->place != comment_trailing)) {
            *prev = comment->next;
            CommentAppend(&taken, comment);
        } else
            prev = &comment->next;
    }
    return taken;
}

/* Note a token, numbered token, of kind kind starting on line */
void
CommentNoteToken(LexToken token, int kind, int line)
{
    /* The window holds tokens the parser may still use, which fit in memory */
    ptrdiff_t index = (ptrdiff_t)(token - commentTokensFirst);

    if (index >= commentTokensSize) {
        commentTokensSize = commentTokensSize ? commentTokensSize * 2 : 256;
        CommentToken *tokens
            = realloc(commentTokens, (size_t)commentTokensSize * sizeof(CommentToken));
        if (!tokens)
            MemExhausted();
        commentTokens = tokens;
    }
    commentTokens[index] = (CommentToken) {
        .line = line,
        .token = kind,
    };
    commentTokensLast = token;
}

/* The line a token whose comments may still be placed starts on, or 0 */
static int
CommentTokenLine(LexToken token)
{
    if (commentTokensFirst <= token && token <= commentTokensLast)
        return commentTokens[(ptrdiff_t)(token - commentTokensFirst)].line;
    return 0;
}

/* The kind of a token whose comments may still be placed, or 0 */
static int
CommentTokenKind(LexToken token)
{
    if (commentTokensFirst <= token && token <= commentTokensLast)
        return commentTokens[(ptrdiff_t)(token - commentTokensFirst)].token;
    return 0;
}

static void CommentForgetNodes(void);

/*
 * Drop the placed comments of tokens before first, those the parser
 * has consumed, with what the parser noted of them
 */
void
CommentReleasePlaced(LexToken first)
{
    while (commentPlaced && commentPlaced->token < first)
        commentPlaced = commentPlaced->next;
    if (first > commentTokensFirst) {
        ptrdiff_t keep = (ptrdiff_t)(commentTokensLast - first + 1);

        if (keep > 0)
            memmove(commentTokens, commentTokens + (ptrdiff_t)(first - commentTokensFirst),
                    (size_t)keep * sizeof(CommentToken));
        commentTokensFirst = first;
    }
    CommentForgetNodes();
}

/* Note the location of the rule being reduced */
void
CommentNoteLocation(LexLocation location)
{
    commentLocation = location;
}

/* Keep an input's file record by its name, replacing the one before */
void
CommentKeepRecord(Atom name, CommentPtr comments)
{
    ENTER();
    CommentFileRecord **prev, *record;

    for (prev = &commentFileRecords; (record = *prev); prev = &record->next)
        if (record->name == name) {
            *prev = record->next;
            break;
        }
    if (comments) {
        record = MemAllocate(&CommentFileRecordType, sizeof(CommentFileRecord));
        record->name = name;
        record->comments = comments;
        record->next = commentFileRecords;
        commentFileRecords = record;
    }
    EXIT();
}

/* The file record of the input called name */
CommentPtr
CommentRecord(Atom name)
{
    CommentFileRecord *record;

    for (record = commentFileRecords; record; record = record->next)
        if (record->name == name)
            return record->comments;
    return 0;
}

void
CommentInit(void)
{
    ENTER();
    commentPlacedReference = NewReference((void **)&commentPlaced);
    MemAddRoot(commentPlacedReference);
    commentFileRecordsReference = NewReference((void **)&commentFileRecords);
    MemAddRoot(commentFileRecordsReference);
    EXIT();
}

/* Put comments into a statement list ahead of rest */
Expr *
CommentList(CommentPtr comments, Expr *rest)
{
    ENTER();
    CommentPtr comment, next;
    Expr      *first = 0, **last = &first;

    for (comment = comments; comment; comment = next) {
        next = comment->next;
        *last = NewExprTree(OC, NewExprComment(comment), 0);
        last = &(*last)->tree.right;
    }
    *last = rest;
    RETURN(first);
}

/*
 * Remove the blank-line entries from comments: all of them, or only
 * one ending the list, which has nothing after it to separate
 */
static CommentPtr
CommentDropBlanks(CommentPtr comments, Bool all)
{
    CommentPtr *prev, comment;

    for (prev = &comments; (comment = *prev);) {
        if (comment->style == comment_blank && (all || !comment->next))
            *prev = comment->next;
        else
            prev = &comment->next;
    }
    return comments;
}

/* Whether an expression holds a comment, which prints with it */
static Bool
CommentInExpr(CommentPtr comment)
{
    return comment->place == comment_before || comment->place == comment_after
        || comment->place == comment_inside_before || comment->place == comment_inside_after
        || comment->place == comment_closing || comment->place == comment_keyword
        || comment->place == comment_type;
}

/*
 * Take the comments of a construct's tokens: those trailing its last
 * token trail it, and the others, before it or inside it, lead it. Blank
 * lines count only before its first token
 */
static void
CommentSplitList(CommentPtr comments, LexLocation location, CommentPtr *leading,
                 CommentPtr *trailing)
{
    CommentPtr  comment, next;
    CommentPtr *lead = leading, *trail = trailing;

    *leading = *trailing = 0;
    for (comment = comments; comment; comment = next) {
        next = comment->next;
        comment->next = 0;
        if (comment->style == comment_blank && comment->token != location.first_token)
            continue;
        if (CommentInExpr(comment))
            continue;
        if (comment->place == comment_trailing && comment->token == location.last_token) {
            *trail = comment;
            trail = &comment->next;
        } else {
            comment->place = comment_leading;
            *lead = comment;
            lead = &comment->next;
        }
    }
}

void
CommentSplit(LexLocation location, CommentPtr *leading, CommentPtr *trailing)
{
    CommentSplitList(CommentTake(location, True), location, leading, trailing);
}

/*
 * The tokens each expression node built during the current command
 * spans, and each declared name's token, found by the node or the
 * declaration through a hash of indexes, so
 * CommentGiveExprs can give the nodes the comments among their
 * tokens. A statement's node is marked: the comments at its edges are
 * its own. CommentReleasePlaced empties the table; MemStack keeps its
 * nodes until then
 */
typedef struct _commentNode {
    void       *key; /* the expression node, or the declared name */
    LexLocation location;
    LexLocation outer; /* with the parentheses around it */
    Bool        statement;
} CommentNode;

static CommentNode *commentNodes;
static ptrdiff_t    commentNodeCount, commentNodeSize;
static ptrdiff_t   *commentNodeHash; /* each a node's index plus one, or 0 */
static ptrdiff_t    commentNodeHashSize;

static ptrdiff_t
CommentNodeSlot(void *key)
{
    return (ptrdiff_t)(((uintptr_t)key >> 4) * 2654435761u & (uintptr_t)(commentNodeHashSize - 1));
}

static CommentNode *
CommentFindNode(void *key)
{
    ptrdiff_t slot;

    if (!commentNodeHashSize)
        return 0;
    for (slot = CommentNodeSlot(key); commentNodeHash[slot];
         slot = (slot + 1) & (commentNodeHashSize - 1))
        if (commentNodes[commentNodeHash[slot] - 1].key == key)
            return &commentNodes[commentNodeHash[slot] - 1];
    return 0;
}

static CommentNode *
CommentAddNode(void *key, LexLocation location)
{
    CommentNode *node = CommentFindNode(key);
    ptrdiff_t    i, slot;

    if (node) {
        node->location = node->outer = location;
        return node;
    }
    if (commentNodeCount == commentNodeSize) {
        commentNodeSize = commentNodeSize ? commentNodeSize * 2 : 1024;
        CommentNode *nodes = realloc(commentNodes, (size_t)commentNodeSize * sizeof(CommentNode));
        if (!nodes)
            MemExhausted();
        commentNodes = nodes;
    }
    /* Keep the hash at most half full */
    if (commentNodeCount * 2 >= commentNodeHashSize) {
        free(commentNodeHash);
        commentNodeHashSize = commentNodeHashSize ? commentNodeHashSize * 2 : 2048;
        commentNodeHash = calloc((size_t)commentNodeHashSize, sizeof(ptrdiff_t));
        if (!commentNodeHash)
            MemExhausted();
        for (i = 0; i < commentNodeCount; i++) {
            for (slot = CommentNodeSlot(commentNodes[i].key); commentNodeHash[slot];
                 slot = (slot + 1) & (commentNodeHashSize - 1))
                ;
            commentNodeHash[slot] = i + 1;
        }
    }
    node = &commentNodes[commentNodeCount++];
    node->key = key;
    node->location = node->outer = location;
    node->statement = False;
    for (slot = CommentNodeSlot(key); commentNodeHash[slot];
         slot = (slot + 1) & (commentNodeHashSize - 1))
        ;
    commentNodeHash[slot] = commentNodeCount;
    return node;
}

static void
CommentForgetNodes(void)
{
    if (commentNodeCount) {
        memset(commentNodeHash, 0, (size_t)commentNodeHashSize * sizeof(ptrdiff_t));
        commentNodeCount = 0;
    }
}

/*
 * Note the tokens a new expression node spans, those of the rule being
 * reduced; a comment entry takes no comments
 */
void
CommentNoteExpr(Expr *e)
{
    if (e->base.tag != COMMENT_CONST && commentLocation.first_token <= commentLocation.last_token)
        CommentAddNode(e, commentLocation);
}

/*
 * Note the tokens of a qualified name's nodes, its rule holding the
 * namespace's '::' too: the name's own token, and the whole name
 */
Expr *
CommentNoteName(Expr *e, LexLocation name, LexLocation span)
{
    Expr *atom = e;

    if (e->base.tag == COLONCOLON) {
        if (span.first_token <= span.last_token)
            CommentAddNode(e, span);
        atom = e->tree.right;
    }
    if (name.first_token <= name.last_token)
        CommentAddNode(atom, name);
    return e;
}

/* Note the parentheses around an expression node, the outermost last */
Expr *
CommentNoteSpan(Expr *e, LexLocation location)
{
    CommentNode *node = CommentFindNode(e);

    if (node)
        node->outer = location;
    return e;
}

/* Whether the source starts a line with the tokens of a location, after a ',' */
static Bool
CommentStartsLine(LexLocation location)
{
    LexToken token = location.first_token;

    return token <= location.last_token && CommentTokenKind(token - 1) == COMMA
        && CommentTokenLine(token - 1) < CommentTokenLine(token);
}

/* Note a declared name's token, and whether a line starts with it after a ',' */
DeclList *
CommentNoteDecl(DeclList *decl, LexLocation name)
{
    if (name.first_token <= name.last_token)
        CommentAddNode(decl, name);
    decl->starts_line = CommentStartsLine(name);
    return decl;
}

/* Note a statement's node and the tokens it spans */
Expr *
CommentNoteStatement(Expr *statement, LexLocation location)
{
    if (statement && location.first_token <= location.last_token)
        CommentAddNode(statement, location)->statement = True;
    return statement;
}

/*
 * Searching a statement for the node a comment belongs to: the comment
 * lies between tokens gap and gap + 1
 */
typedef struct _commentGap {
    LexToken     gap;
    CommentPlace place;    /* looking for a node starting after the gap, ending before it, or
                              closing after it */
    Expr       **found;    /* the comments of the node or declared name found */
    Bool         inside;   /* whether it's just inside the parentheses around the node */
    Bool         one_line; /* whether the comment was written on one line */
    ptrdiff_t    after;    /* the type tokens after one in a declaration's type */
} CommentGap;

/*
 * Find the declared name a gap's comment goes before or after. A
 * function's name prints with its code, which print shows from the
 * symbol too, so its code's name takes the comment, when it was written
 * on one line and so prints there; the statement keeps one that isn't.
 * A function declared without a body has no code, so its name takes it
 */
static Bool
CommentFindDecl(Expr *e, CommentGap *g)
{
    DeclListPtr  decl;
    CommentNode *node;

    for (decl = e->decl.decl; decl; decl = decl->next) {
        node = CommentFindNode(decl);
        if (!node
            || !((g->place == comment_before && node->location.first_token == g->gap + 1)
                 || (g->place == comment_after && node->location.last_token == g->gap)))
            continue;
        if (e->base.tag == VAR || !decl->init)
            g->found = &decl->comments;
        else if (decl->init->base.tag == FUNC && g->one_line)
            g->found = &decl->init->code.code->base.name->base.comments;
        return True;
    }
    return False;
}

static Bool
CommentFindGap(Expr *e, void *closure)
{
    CommentGap  *g = closure;
    CommentNode *node = CommentFindNode(e);

    if (!node)
        return False;
    /* A nested statement keeps the comments at its edges, as it does in a list */
    if (node->statement) {
        if (e->base.tag == FUNC && CommentFindDecl(e, g))
            return True;
        if (node->location.first_token == g->gap + 1 || node->location.last_token == g->gap)
            return True;
        return False;
    }
    /*
     * A list's nodes span tokens that rehanging it rearranged; a for's
     * clauses are a list; and the return of a body written '= value'
     * spans its value, which prints the comments
     */
    if (e->base.tag == COMMA || e->base.tag == SEMI || e->base.tag == RETURNTOK)
        return False;
    /*
     * A declaration prints through its names and values, after its
     * type; one just before it, as after a for's '(', goes before it
     */
    if (e->base.tag == VAR) {
        if (CommentFindDecl(e, g))
            return True;
        if (g->place != comment_before || node->location.first_token != g->gap + 1)
            return False;
        g->found = &e->base.comments;
        return True;
    }
    /* Outside its parentheses, or just inside them */
    switch (g->place) {
    case comment_before:
        if (node->outer.first_token == g->gap + 1)
            g->inside = False;
        else if (node->location.first_token == g->gap + 1)
            g->inside = True;
        else
            return False;
        break;
    case comment_after:
        if (node->outer.last_token == g->gap)
            g->inside = False;
        else if (node->location.last_token == g->gap)
            g->inside = True;
        else
            return False;
        break;
    default:
        /* The innermost node ending with the next token: keep looking inside */
        if (node->location.first_token <= g->gap && node->location.last_token == g->gap + 1)
            g->found = &e->base.comments;
        return False;
    }
    g->found = &e->base.comments;
    return True;
}

/*
 * Find the declaration, typedef, or function definition whose type,
 * before its first name, or the initializer whose type, in parentheses
 * before its value, holds a gap, counting the type's tokens after it.
 * One before a struct's '}' leads the statement, as a struct may print
 * on one line
 */
static Bool
CommentFindType(Expr *e, void *closure)
{
    CommentGap  *g = closure;
    CommentNode *node, *end;
    LexToken     last;
    Expr       **found = &e->base.comments, *init;

    node = CommentFindNode(e);
    if (!node)
        return False;
    switch (e->base.tag) {
    case FUNC:
        /*
         * A definition's return type prints with its code, which takes
         * the comment when written on one line, as its name does; a
         * lambda is a FUNC node too, but no statement
         */
        if (!node->statement)
            return False;
        init = e->decl.decl ? e->decl.decl->init : 0;
        end = CommentFindNode(e->decl.decl);
        if (!g->one_line || !init || init->base.tag != FUNC || !init->code.code->base.name || !end)
            return False;
        found = &init->code.code->base.name->base.comments;
        last = end->location.first_token - 1;
        break;
    case TYPEDEF:
        /* A typedef's statement holds the declaration of its names */
        if (node->statement)
            return False;
        __NICKLE_FALLTHROUGH;
    case VAR:
        end = CommentFindNode(e->decl.decl);
        if (!end)
            return False;
        last = end->location.first_token - 1;
        break;
    case NEW:
        /* Its type lies between '(' and the ')' before its value */
        end = e->tree.left ? CommentFindNode(e->tree.left) : 0;
        if (!end)
            return False;
        last = end->outer.first_token - 2;
        break;
    default:
        return False;
    }
    if (g->gap < node->location.first_token || g->gap > last || CommentTokenKind(g->gap + 1) == CC)
        return False;
    g->found = found;
    g->after = (ptrdiff_t)(last - g->gap);
    return True;
}

/*
 * Searching for the innermost control statement holding the tokens on
 * both sides of a gap, one whose printer places comments after its own
 * tokens
 */
typedef struct _commentHolder {
    LexToken gap;
    Expr    *found;
} CommentHolder;

static Bool
CommentFindHolder(Expr *e, void *closure)
{
    CommentHolder *h = closure;
    CommentNode   *node = CommentFindNode(e);

    if (!node || !node->statement || node->location.first_token > h->gap
        || node->location.last_token <= h->gap)
        return False;
    switch (e->base.tag) {
    case IF:
    case ELSE:
    case WHILE:
    case DO:
    case FOR:
    case SWITCH:
    case UNION:
    case ENUM:
    case RETURNTOK:
    case BREAK:
    case CONTINUE:
    case RAISE:
        h->found = e;
        break;
    default:
        /* Others, as declarations, print their comments with the statement */
        h->found = 0;
        break;
    }
    return False;
}

/*
 * Give a name the rule building its node took, as a member's after '.'
 * or '->' or a struct initializer's before '=', its own token: the
 * name's node spans the rule's tokens, as the node holding it does
 */
static Bool
CommentRuleNames(Expr *e, void *closure)
{
    CommentNode *node, *atom;
    Expr        *name;
    LexToken     token;

    (void)closure;
    node = CommentFindNode(e);
    if (!node)
        return False;
    switch (e->base.tag) {
    case DOT:
    case ARROW:
        name = e->tree.right;
        token = node->location.last_token;
        break;
    case ASSIGN:
        name = e->tree.left;
        token = node->location.first_token;
        if (CommentTokenKind(token) == DOT)
            token++;
        break;
    default:
        return False;
    }
    atom = name && name->base.tag == NAME ? CommentFindNode(name) : 0;
    if (!atom || atom->location.first_token != node->location.first_token
        || atom->location.last_token != node->location.last_token)
        return False;
    atom->location.first_token = atom->location.last_token = token;
    atom->location.first_line = atom->location.last_line = CommentTokenLine(token);
    atom->outer = atom->location;
    return False;
}

/* Add a comment entry to the end of a node's or a declared name's comments */
static void
CommentAddTo(Expr **comments, CommentPtr comment)
{
    ENTER();
    Expr **last;

    for (last = comments; *last; last = &(*last)->tree.right)
        ;
    *last = NewExprTree(OC, NewExprComment(comment), 0);
    EXIT();
}

/* Whether a token closes or separates what it's in */
static Bool
CommentClosingToken(LexToken token)
{
    switch (CommentTokenKind(token)) {
    case COMMA:
    case SEMI:
    case CP:
    case CS:
    case CC:
        return True;
    }
    return False;
}

/*
 * Give the expression nodes and declared names of a statement the
 * comments among their tokens. A comment with code before it on its line trails that code,
 * unless code other than a closing or separating token follows it on
 * its line: then, like a comment on a line of its own, it leads the
 * code after it. One leading code goes before the outermost node
 * starting after it, else after the outermost one ending before it;
 * one trailing code tries those the other way around; and when neither
 * is found, one goes before the closing token of the innermost node it
 * ends, as after an initializer's last comma. Those at the statement's
 * edges, and those no node takes, stay for the statement. The comments
 * stay placed, which holds them while the nodes taking them allocate
 */
static void
CommentGiveExprs(Expr *statement, LexLocation location)
{
    CommentPtr comment;
    CommentGap g;
    int        before, after;
    Bool       trails;

    ExprVisitPreOrder(statement, CommentRuleNames, 0);
    for (comment = commentPlaced; comment; comment = comment->next) {
        if (comment->token < location.first_token || location.last_token < comment->token
            || comment->place == comment_file || comment->place == comment_type
            || comment->style == comment_blank)
            continue;
        g.gap = comment->place == comment_trailing ? comment->token : comment->token - 1;
        g.found = 0;
        g.inside = False;
        g.one_line = comment->first_line == comment->last_line;
        /*
         * Past the last token only one trailing it before a closing token,
         * as a body written '= value;' spans its value but not the ';'
         */
        if (g.gap < location.first_token || g.gap > location.last_token
            || (g.gap == location.last_token
                && (comment->place != comment_trailing || !CommentClosingToken(g.gap + 1))))
            continue;
        before = CommentTokenLine(g.gap);
        after = CommentTokenLine(g.gap + 1);
        trails = comment->place == comment_trailing
            && (!after || comment->last_line < after || CommentClosingToken(g.gap + 1));
        g.place = trails ? comment_after : comment_before;
        if (!ExprVisitPreOrder(statement, CommentFindGap, &g)) {
            g.place = trails ? comment_before : comment_after;
            if (!ExprVisitPreOrder(statement, CommentFindGap, &g)) {
                g.place = comment_closing;
                ExprVisitPreOrder(statement, CommentFindGap, &g);
            }
        }
        /* One in a declaration's type goes among the type's tokens */
        if (!g.found && ExprVisitPreOrder(statement, CommentFindType, &g)) {
            g.place = comment_type;
            comment->after = g.after;
        }
        /*
         * One no expression takes, between a control statement's own
         * tokens, follows the token before it
         */
        if (!g.found) {
            CommentHolder h = {
                .gap = g.gap,
                .found = 0,
            };

            ExprVisitPreOrder(statement, CommentFindHolder, &h);
            if (!h.found)
                continue;
            g.found = &h.found->base.comments;
            g.place = comment_keyword;
            comment->keyword = CommentTokenKind(g.gap);
        }
        comment->starts_line
            = comment->place != comment_trailing && (!before || comment->first_line > before);
        /* A token closing what it's in may follow it on its line */
        comment->ends_line
            = (!after || comment->last_line < after) && !CommentClosingToken(g.gap + 1);
        comment->place = g.place;
        if (g.inside && g.place == comment_before)
            comment->place = comment_inside_before;
        else if (g.inside && g.place == comment_after)
            comment->place = comment_inside_after;
        CommentAddTo(g.found, comment);
    }
}

/*
 * Take the comments of a statement's tokens, giving its expressions
 * those among their tokens, and split the rest as CommentSplit
 * does
 */
void
CommentSplitStatement(Expr *statement, LexLocation location, CommentPtr *leading,
                      CommentPtr *trailing)
{
    CommentGiveExprs(statement, location);
    CommentSplitList(CommentTake(location, True), location, leading, trailing);
}

/* Append copies of comment entries at *last */
static void
CommentCopyEntries(Expr *c, Expr ***last)
{
    for (; c; c = c->tree.right) {
        **last = NewExprTree(OC, c->tree.left, 0);
        *last = &(**last)->tree.right;
    }
}

/* Collect the comments an expression and the names it declares hold */
static Bool
CommentCollectHeld(Expr *e, void *closure)
{
    DeclListPtr decl;

    CommentCopyEntries(e->base.comments, closure);
    if (e->base.tag == VAR)
        for (decl = e->decl.decl; decl; decl = decl->next)
            CommentCopyEntries(decl->comments, closure);
    return False;
}

/*
 * Give the global symbols a declaration declares its comments, which
 * print with them; every symbol of the declaration gets them all. A
 * variable prints its value, not the expressions it was declared
 * with, so the comments those hold print with the leading ones
 */
void
CommentSymbols(Expr *statement, CommentPtr leading, CommentPtr trailing)
{
    ENTER();
    DeclListPtr decl = 0;
    SymbolPtr   symbol;
    Expr       *held = 0, **last = &held, *comments;

    switch (statement->base.tag) {
    case FUNC:
        decl = statement->decl.decl;
        break;
    case EXCEPTION:
        /* Its doc comments print in it */
        decl = statement->decl.decl;
        CommentCopyEntries(decl->comments, &last);
        break;
    case EXPR:
        /* A variable declaration is an expression statement */
        if (statement->tree.left->base.tag == VAR) {
            decl = statement->tree.left->decl.decl;
            ExprVisitPreOrder(statement->tree.left, CommentCollectHeld, &last);
        }
        break;
    case TYPEDEF:
        /* Printing a typedef shows its type without the comments among its tokens */
        decl = statement->tree.left->decl.decl;
        ExprVisitPreOrder(statement->tree.left, CommentCollectHeld, &last);
        break;
    }
    if (!leading && !trailing && !held) {
        EXIT();
        return;
    }
    *last = CommentList(trailing, NewExprTree(OC, 0, 0));
    comments = CommentList(leading, held);
    if (statement->base.tag == NAMESPACE) {
        symbol = statement->tree.left->decl.decl->symbol;
        if (symbol)
            symbol->symbol.comments = comments;
    }
    for (; decl; decl = decl->next) {
        symbol = decl->symbol;
        if (!symbol)
            continue;
        switch (symbol->symbol.class) {
        case class_global:
        case class_const:
        case class_typedef:
        case class_exception:
            symbol->symbol.comments = comments;
            break;
        default:
            break;
        }
    }
    EXIT();
}

/*
 * The comment entries of a construct's tokens, leading then trailing,
 * or 0 when it has none
 */
Expr *
CommentEntries(LexLocation location)
{
    ENTER();
    CommentPtr leading, trailing;

    CommentSplit(location, &leading, &trailing);
    if (!leading && !trailing)
        RETURN(0);
    RETURN(CommentList(leading, CommentList(trailing, NewExprTree(OC, 0, 0))));
}

/*
 * The comment entries of an item in a comma-separated list and its
 * comma: those trailing the item or the comma trail it, and the
 * others lead it
 */
Expr *
CommentItemEntries(LexLocation item, LexLocation comma)
{
    ENTER();
    CommentPtr  leading, trailing, comma_leading, comma_trailing;
    CommentPtr *end;

    CommentSplit(item, &leading, &trailing);
    CommentSplit(comma, &comma_leading, &comma_trailing);
    comma_leading = CommentDropBlanks(comma_leading, True);
    for (end = &leading; *end; end = &(*end)->next)
        ;
    *end = comma_leading;
    for (end = &trailing; *end; end = &(*end)->next)
        ;
    *end = comma_trailing;
    if (!leading && !trailing)
        RETURN(0);
    RETURN(CommentList(leading, CommentList(trailing, NewExprTree(OC, 0, 0))));
}

/*
 * Give comment entries the comments among a type's tokens and after
 * them, as comment_type, each counting the type's tokens after it,
 * apart from those before a struct's '}'. One between a '(' or '{' and
 * the type, code following it on its line, as before a first parameter
 * or member, goes before the type
 */
static Expr *
CommentTypeEntries(LexLocation type)
{
    ENTER();
    Expr      *entries = 0;
    CommentPtr comment;
    LexToken   gap;

    if (type.first_token > type.last_token)
        RETURN(0);
    for (comment = commentPlaced; comment; comment = comment->next) {
        if (comment->place != comment_leading && comment->place != comment_trailing)
            continue;
        if (comment->style == comment_blank)
            continue;
        gap = comment->place == comment_trailing ? comment->token : comment->token - 1;
        if (gap == type.first_token - 1) {
            if ((CommentTokenKind(gap) != OP && CommentTokenKind(gap) != OC)
                || comment->last_line != CommentTokenLine(type.first_token))
                continue;
        } else if (gap < type.first_token || gap > type.last_token
                   || CommentTokenKind(gap + 1) == CC)
            continue;
        comment->place = comment_type;
        comment->after = (ptrdiff_t)(type.last_token - gap);
        CommentAddTo(&entries, comment);
    }
    RETURN(entries);
}

/*
 * Give a parameter the comments of its declaration and its comma, as
 * CommentItemEntries does, those among its type's tokens first,
 * and note whether a line starts with it, after a ','. One before a
 * last parameter's '...' leads it, as one after the '...' can't trail
 * it: the parser hasn't read those yet
 */
ArgType *
CommentArg(ArgType *arg, LexLocation item, LexLocation comma)
{
    ENTER();
    Expr      **end = &arg->comments;

    LexLocation type = item;

    arg->starts_line = CommentStartsLine(item);
    if (arg->name)
        type.last_token--;
    for (*end = CommentTypeEntries(type); *end; end = &(*end)->tree.right)
        ;
    if (arg->varargs)
        *end = CommentEntries((LexLocation) {
            .first_token = item.first_token,
            .last_token = comma.last_token,
            .first_line = item.first_line,
            .last_line = comma.last_line,
        });
    else
        *end = CommentItemEntries(item, comma);
    RETURN(arg);
}

/*
 * Give each name of a struct or union member declaration the
 * declaration's comments, around those of the name itself, the first
 * name taking those among the type's tokens before them, and the first
 * member those after the '{'
 */
void
CommentMembers(AtomListPtr atoms, LexLocation type, LexLocation location)
{
    ENTER();
    CommentPtr  leading, trailing;
    AtomListPtr al;
    Expr      **end, *types = 0;

    CommentPtr  comment;

    types = CommentTypeEntries(type);
    /* One ending the line of the '{' before the first member leads it */
    for (comment = commentPlaced; comment; comment = comment->next)
        if (comment->place == comment_trailing && comment->token == location.first_token - 1
            && CommentTokenKind(comment->token) == OC) {
            comment->place = comment_leading;
            comment->token = location.first_token;
        }
    CommentSplit(location, &leading, &trailing);
    if (leading || trailing) {
        for (al = atoms; al; al = al->next) {
            Expr *entries = al->comments ? al->comments : NewExprTree(OC, 0, 0);

            for (end = &entries; (*end)->tree.left; end = &(*end)->tree.right)
                ;
            *end = CommentList(trailing, *end);
            al->comments = CommentList(leading, entries);
        }
    }
    if (types && atoms) {
        for (end = &types; *end; end = &(*end)->tree.right)
            ;
        *end = atoms->comments;
        atoms->comments = types;
    }
    EXIT();
}

/* Record the comments of a struct, union, or enum's members */
void
CommentStruct(StructType *st, AtomListPtr atoms)
{
    ENTER();
    AtomListPtr al;

    for (al = atoms; al; al = al->next)
        if (al->comments)
            st->comments = NewExprTree(
                OC, NewExprTree(CASE, NewExprAtom(al->atom, 0, False), al->comments), st->comments);
    EXIT();
}

/*
 * Make the comments after a function's, catch's, or exception's
 * header, those trailing its ')' and those leading the '{', '=', or ';'
 * after it, its doc comments, which print under the header, one level
 * in. They start its body, returned, or with none follow the comments
 * of its declared name
 */
Expr *
CommentDoc(LexLocation header, Expr *body, DeclList *decl)
{
    ENTER();
    LexToken   close = header.last_token;
    CommentPtr docs = 0, comment, *prev;
    Expr     **last;

    for (prev = &commentPlaced; (comment = *prev);) {
        if ((comment->token == close && comment->place == comment_trailing)
            || (comment->token == close + 1 && comment->place == comment_leading)) {
            *prev = comment->next;
            CommentAppend(&docs, comment);
        } else
            prev = &comment->next;
    }
    /* The body's statements took those leading its '{' or '=' */
    while (body && body->tree.left && body->tree.left->base.tag == COMMENT_CONST
           && (comment = body->tree.left->comment.comment)->token == close + 1
           && comment->place == comment_leading) {
        CommentAppend(&docs, comment);
        body = body->tree.right;
    }
    docs = CommentDropBlanks(docs, True);
    for (comment = docs; comment; comment = comment->next)
        comment->place = comment_doc;
    if (body)
        RETURN(CommentList(docs, body));
    if (decl) {
        for (last = &decl->comments; *last; last = &(*last)->tree.right)
            ;
        *last = CommentList(docs, 0);
    }
    RETURN(0);
}

/*
 * Put a statement into a statement list with its comments: those
 * trailing its last token follow it, and the others, before it or
 * inside it, lead it. A declaration's global symbols take them too
 */
Expr *
CommentStatementEntry(Expr *statement, LexLocation location, Expr *rest)
{
    ENTER();
    CommentPtr leading, trailing;

    CommentSplitStatement(statement, location, &leading, &trailing);
    CommentSymbols(statement, leading, trailing);
    RETURN(CommentList(leading, NewExprTree(OC, statement, CommentList(trailing, rest))));
}

/*
 * Give a braced statement list the comments of its braces: those
 * trailing or leading the opening brace start the list, and those
 * leading the closing brace end it
 */
Expr *
CommentBlock(Expr *statements, LexLocation open, LexLocation close)
{
    ENTER();
    CommentPtr comment, opening = CommentDropBlanks(CommentTake(open, True), True);
    Expr      *end, **last;

    /* Those trailing the brace lead what follows it */
    for (comment = opening; comment; comment = comment->next)
        if (comment->place == comment_trailing) {
            comment->place = comment_leading;
            comment->token = open.last_token + 1;
        }
    statements = CommentList(opening, statements);
    for (last = &statements; (*last)->tree.left; last = &(*last)->tree.right)
        ;
    end = *last;
    *last = CommentList(CommentDropBlanks(CommentTake(close, False), False), end);
    RETURN(statements);
}

/* Put comments into a switch's case list ahead of rest */
static Expr *
CommentCaseList(CommentPtr comments, Expr *rest)
{
    ENTER();
    CommentPtr comment, next;
    Expr      *first = 0, **last = &first;

    for (comment = comments; comment; comment = next) {
        next = comment->next;
        comment->next = 0;
        comment->place = comment_leading;
        *last = NewExprTree(CASE, NewExprComment(comment), 0);
        last = &(*last)->tree.right;
    }
    *last = rest;
    RETURN(first);
}

/*
 * Put a case into a switch's case list with the comments its
 * statements didn't take: those before its label go in the case list
 * ahead of it, and the others, in or after the label, start its
 * statements, those trailing the label first. Fall-through comments, '[[fallthrough]]' alone,
 * starting the comments before the next case's label end this case's statements instead, as they
 * describe its end, dropping a blank line above them
 */
Expr *
CommentCaseEntry(Expr *c, LexLocation location, Expr *rest)
{
    ENTER();
    /* The label's expressions take the comments among their tokens */
    if (c->tree.left)
        CommentGiveExprs(c->tree.left, location);
    CommentPtr comments = CommentTake(location, True);
    CommentPtr before = 0, *bef = &before;
    CommentPtr after = 0, *aft = &after;
    CommentPtr comment, next;
    Expr     **end;
    Bool       trails = True;

    for (comment = comments; comment; comment = next) {
        next = comment->next;
        comment->next = 0;
        if (CommentInExpr(comment))
            continue;
        if (comment->place == comment_leading && comment->token == location.first_token) {
            *bef = comment;
            bef = &comment->next;
        } else {
            /* Those trailing the label stay trailing, so they print after it */
            trails = trails && comment->place == comment_trailing;
            if (!trails)
                comment->place = comment_leading;
            *aft = comment;
            aft = &comment->next;
        }
    }
    c->tree.right = CommentList(CommentDropBlanks(after, True), c->tree.right);
    for (end = &c->tree.right; *end && (*end)->tree.left; end = &(*end)->tree.right)
        ;
    for (;;) {
        Expr *mark = rest;

        /* A blank line before a fall-through mark goes with it */
        if (mark && mark->tree.left->base.tag == COMMENT_CONST
            && mark->tree.left->comment.comment->style == comment_blank)
            mark = mark->tree.right;
        if (!mark || mark->tree.left->base.tag != COMMENT_CONST
            || !mark->tree.left->comment.comment->fall_through)
            break;
        *end = NewExprTree(OC, mark->tree.left, *end);
        end = &(*end)->tree.right;
        rest = mark->tree.right;
    }
    RETURN(CommentCaseList(before, NewExprTree(CASE, c, rest)));
}

/*
 * Give a switch's case list the comments of its braces: those trailing
 * or leading the opening brace start the list, and those leading the
 * closing brace end it
 */
Expr *
CommentCaseBlock(Expr *cases, LexLocation open, LexLocation close)
{
    ENTER();
    Expr **last;

    cases = CommentCaseList(CommentDropBlanks(CommentTake(open, True), True), cases);
    for (last = &cases; *last; last = &(*last)->tree.right)
        ;
    *last = CommentCaseList(CommentDropBlanks(CommentTake(close, False), False), 0);
    RETURN(cases);
}
