/*
 * Copyright © 2026 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * Comments, for the pretty-printer (comment.c), and the parser
 * locations that place them. Included by nickle.h after the
 * expression and declaration types, which these use
 */

#ifndef _COMMENT_H_
#define _COMMENT_H_

/*
 * Comments, recorded as they are read and placed by position alone.
 * The comments before an input's first token, except one ending on the
 * line just above that token, and those after its last token make up
 * the input's file record. A comment starting on the line of the
 * previous token trails that token; any other comment leads the next
 * one. Tokens are numbered across all inputs, and a placed comment
 * holds the number of the token it trails or leads
 */
typedef enum _commentStyle {
    comment_block,   /* slash-star comment */
    comment_hash,    /* run of lines starting with '#' */
    comment_shebang, /* '#' run starting with '#!' on line 1 */
    comment_blank    /* blank lines between code or comments, with no text */
} CommentStyle;

/*
 * An expression's comments go before it, after it, just inside the
 * parentheses around it, or before its closing token; a statement's
 * after one of its own tokens, as 'if' or its header's ')'; a
 * declaration's or parameter's among its type's tokens; and a
 * function's, catch's, or exception's doc comments after its header
 */
typedef enum _commentPlace {
    comment_file,
    comment_leading,
    comment_trailing,
    comment_before,
    comment_after,
    comment_inside_before,
    comment_inside_after,
    comment_closing,
    comment_keyword,
    comment_type,
    comment_doc
} CommentPlace;

/*
 * A token's number, counting tokens across all inputs for the life of
 * the process, so 64 bits even where ptrdiff_t has 32
 */
typedef int64_t LexToken;

typedef struct _comment {
    DataType        *data;
    struct _comment *next;
    /* As written, from the opening mark; a string file while '#' lines join */
    Value            text;
    int              first_line;
    int              last_line;
    CommentStyle     style;
    CommentPlace     place;
    LexToken         token;
    int              keyword;      /* the kind of token a comment_keyword one follows */
    ptrdiff_t        after;        /* the type tokens after a comment_type one */
    Bool             fall_through; /* marks a case falling through, as GCC reads one */
    /* Inside an expression, whether no code precedes or follows it on its lines */
    Bool             starts_line;
    Bool             ends_line;
    Bool             printed; /* set when an expression's printing placed it */
} Comment, *CommentPtr;

/*
 * Parser locations, which the parser generator tracks for each symbol
 * (%locations): the tokens from first_token through last_token. An
 * empty symbol's location lies just before the next token, with
 * last_token one less than first_token. first_line and last_line are
 * the source lines those tokens start and end on, an empty symbol's
 * both the line before it ends on
 */
typedef struct _lexLocation {
    LexToken first_token;
    LexToken last_token;
    int      first_line;
    int      last_line;
} LexLocation;

#define YYLTYPE LexLocation

/*
 * An input's comments, kept in the lexer's record of the input: those
 * read since its last token, waiting for the next one to place them,
 * and its file record
 */
typedef struct _commentInput {
    LexToken tokens;        /* tokens read from this input */
    int      first_line;    /* line of its first token */
    int      token_line;    /* line of the last one */
    int      content_line;  /* last line of the last token or comment */
    Comment *pending;       /* comments waiting for the next token */
    Comment *file_comments; /* the file record */
} CommentInput;

/* comment.c */
void       CommentInit(void);
void       CommentInputToken(CommentInput *in, LexToken token, int line);
void       CommentInputRecord(CommentInput *in, Value text, int first_line, int last_line,
                              CommentStyle style, Bool fall_through, LexToken token);
CommentPtr CommentInputFinish(CommentInput *in);
void       CommentNoteExpr(Expr *e);
CommentPtr NewComment(Value text, int first_line, int last_line, CommentStyle style,
                      Bool fall_through);
void       CommentAppend(CommentPtr *list, CommentPtr comment);
void       CommentAddPlaced(CommentPtr comment, CommentPlace place, LexToken token);
void       CommentReleasePlaced(LexToken first);
void       CommentNoteToken(LexToken token, int kind, int line);
void       CommentNoteLocation(LexLocation location);
Expr      *CommentNoteName(Expr *e, LexLocation name, LexLocation span);
Expr      *CommentNoteSpan(Expr *e, LexLocation location);
DeclList  *CommentNoteDecl(DeclList *decl, LexLocation name);
Expr      *CommentNoteStatement(Expr *statement, LexLocation location);
void       CommentKeepRecord(Atom name, CommentPtr comments);
CommentPtr CommentTake(LexLocation location, Bool trailing);
CommentPtr CommentRecord(Atom name);
Expr      *CommentList(CommentPtr comments, Expr *rest);
void       CommentSplit(LexLocation location, CommentPtr *leading, CommentPtr *trailing);
void       CommentSplitStatement(Expr *statement, LexLocation location, CommentPtr *leading,
                                 CommentPtr *trailing);
void       CommentSymbols(Expr *statement, CommentPtr leading, CommentPtr trailing);
Expr      *CommentEntries(LexLocation location);
Expr      *CommentItemEntries(LexLocation item, LexLocation comma);
ArgType   *CommentArg(ArgType *arg, LexLocation item, LexLocation comma);
void       CommentMembers(AtomListPtr atoms, LexLocation type, LexLocation location);
void       CommentStruct(StructType *st, AtomListPtr atoms);
Expr      *CommentDoc(LexLocation header, Expr *body, DeclList *decl);
Expr      *CommentStatementEntry(Expr *statement, LexLocation location, Expr *rest);
Expr      *CommentBlock(Expr *statements, LexLocation open, LexLocation close);
Expr      *CommentCaseEntry(Expr *c, LexLocation location, Expr *rest);
Expr      *CommentCaseBlock(Expr *cases, LexLocation open, LexLocation close);

#endif /* _COMMENT_H_ */
