#define CONST 257
#define NL 258
#define ALL 259
#define DOWN 260
#define UP 261
#define QUIT 262
#define READ 263
#define SHELL 264
#define EDIT 265
#define DEFAULT 266
#define UNDEFINE 267
#define HISTORY 268
#define PRINT 269
#define WHILE 270
#define IF 271
#define ELSE 272
#define FOR 273
#define DO 274
#define BREAK 275
#define CONTINUE 276
#define EXPR 277
#define RETURNTOK 278
#define SCOPE 279
#define IMPORT 280
#define TRY 281
#define CATCH 282
#define FORK 283
#define OP 284
#define CP 285
#define OC 286
#define CC 287
#define COMMA 288
#define SEMI 289
#define DOLLAR 290
#define VAR 291
#define ASSIGNPLUS 292
#define ASSIGNMINUS 293
#define ASSIGNTIMES 294
#define ASSIGNDIVIDE 295
#define ASSIGNDIV 296
#define ASSIGNMOD 297
#define ASSIGNPOW 298
#define ASSIGNLXOR 299
#define ASSIGNLAND 300
#define ASSIGNLOR 301
#define NAME 302
#define AUTO 303
#define GLOBAL 304
#define STATIC 305
#define PUBLIC 306
#define INT 307
#define NATURAL 308
#define INTEGER 309
#define RATIONAL 310
#define DOUBLE 311
#define ARRAY 312
#define FUNC 313
#define EXCEPTION 314
#define STRING 315
#define POLYMORPH 316
#define REF 317
#define STRUCT 318
#define TYPE 319
#define FUNCTION 320
#define AINIT 321
#define POUND 322
#define COMMAOP 323
#define ASSIGN 324
#define QUEST 325
#define COLON 326
#define OR 327
#define AND 328
#define LOR 329
#define LXOR 330
#define LAND 331
#define EQ 332
#define NE 333
#define LT 334
#define GT 335
#define LE 336
#define GE 337
#define PLUS 338
#define MINUS 339
#define TIMES 340
#define DIVIDE 341
#define DIV 342
#define MOD 343
#define POW 344
#define UMINUS 345
#define BANG 346
#define FACT 347
#define LNOT 348
#define INC 349
#define DEC 350
#define STAR 351
#define AMPER 352
#define THREAD 353
#define OS 354
#define CS 355
#define DOT 356
#define ARROW 357
#define CALL 358
typedef union {
    int		    ival;
    Value	    vval;
    Class	    cval;
    Type	    tval;
    Publish	    pval;
    ExprPtr	    eval;
    Atom	    aval;
    DeclListPtr	    dval;
    MemListPtr	    mval;
    Fulltype	    ftval;
    ScopePtr	    sval;
    CodePtr	    fval;
} YYSTYPE;
extern YYSTYPE yylval;
