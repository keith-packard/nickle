/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * pretty.c
 *
 * pretty print a function
 */

#include	<stdio.h>
#include	"nickle.h"
#include	"gram.h"

#define PRETTY_NUM_WIDTH    10

static int
PrettyNumWidth (unsigned long i)
{
    int	w = 1;
    while (i >= 10)
    {
	w++;
	i /= 10;
    }
    return w;
}

static void
PrettyProfNum (Value f, unsigned long i, int pad_left)
{
    int	spaces = PRETTY_NUM_WIDTH - PrettyNumWidth (i);
    if (pad_left)
	while (spaces-- > 0)
	    FilePuts (f, " ");
    FilePrintf (f, "%d", i);
    if (!pad_left)
	while (spaces-- > 0)
	    FilePuts (f, " ");
}
	    
static void PrettyParameters (Value f, Expr *e, Bool nest, ProfileData *pd);
static void PrettyArrayInit (Value f, Expr *e, int level, Bool nest, ProfileData *pd);
static void PrettyStatement (Value f, Expr *e, int level, int blevel, Bool nest, ProfileData *pd);
static void PrettyBody (Value f, CodePtr code, int level, Bool nest, ProfileData *pd);

static void
PrettyIndent (Value f, Expr *e, int level, ProfileData *pd)
{
    int	i;
    if (profiling)
    {
	if (e)
	{
	    PrettyProfNum (f, e->base.sub_ticks, 1);
	    FilePuts (f, " ");
	    PrettyProfNum (f, e->base.ticks, 1);
	    if (pd)
	    {
		pd->sub += e->base.sub_ticks;
		pd->self += e->base.ticks;
	    }
	}
	else
	    FilePuts (f, "                     ");
        FilePuts (f, ":  ");
    }
    for (i = 0; i < level-1; i += 2)
	FileOutput (f, '\t');
    if (i < level)
	FilePuts (f, "    ");
}

static void
PrettyBlock (Value f, Expr *e, int level, Bool nest, ProfileData *pd)
{
    while (e->tree.left) {
	PrettyStatement (f, e->tree.left, level, level, nest, pd);
	e = e->tree.right;
    }
}

static int
tokenToPrecedence (int token)
{
    switch (token) {
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
	return 1;
    case QUEST: case COLON:
	return 2;
    case OR:
	return 3;
    case AND:
	return 4;
    case LOR:
	return 5;
    case LAND:
	return 6;
    case LXOR:
	return 7;
    case EQ: case NE:
	return 8;
    case LT: case GT: case LE: case GE:
	return 9;
    case SHIFTL: case SHIFTR:
	return 10;
    case PLUS: case MINUS:
	return 11;
    case TIMES: case DIVIDE: case MOD: case DIV:
	return 12;
    case POW:
	return 13;
    case UMINUS: case BANG: case FACT: case LNOT:
	return 14;
    case INC: case DEC:
	return 15;
    case STAR: case AMPER:
	return 16;
    case OS: case CS:
	return 17;
    default:
	return 18;
    }
}

static void
PrettyParameters (Value f, Expr *e, Bool nest, ProfileData *pd)
{
    while (e) 
    {
	if (e->tree.left->base.tag == DOTS)
	{
	    PrettyExpr (f, e->tree.left->tree.left, -1, 0, nest, pd);
	    FilePuts (f, "...");
	}
	else
	{
	    PrettyExpr (f, e->tree.left, -1, 0, nest, pd);
	}
	e = e->tree.right;
	if (e)
	    FilePuts (f, ", ");
    }
}

static void
PrettyArrayInit (Value f, Expr *e, int level, Bool nest, ProfileData *pd);

static void
PrettyArrayInits (Value f, Expr *e, int level, Bool nest, ProfileData *pd)
{
    while (e)
    {
	if (e->tree.left)
	    PrettyArrayInit (f, e->tree.left, 0, nest, pd);
	e = e->tree.right;
	if (e)
	{
	    if (e->tree.left->base.tag == DOTS)
		FilePuts (f, " ");
	    else
		FilePuts (f, ", ");
	}
    }
}

static void
PrettyArrayInit (Value f, Expr *e, int level, Bool nest, ProfileData *pd)
{
    switch (e->base.tag) {
    case OC:
	FilePuts (f, "{ ");
	PrettyArrayInits (f, e->tree.left, level, nest, pd);
	FilePuts (f, " }");
	break;
    case DOTS:
	FilePuts (f, "...");
	break;
    default:
	PrettyExpr (f, e, -1, level, nest, pd);
	break;
    }
}

static void
PrettyStructInit (Value f, Expr *e, int level, Bool nest, ProfileData *pd)
{
    while (e)
    {
	FilePuts (f, AtomName (e->tree.left->tree.left->atom.atom));
	FilePuts (f, " = ");
	PrettyExpr (f, e->tree.left->tree.right, -1, level, nest, pd);
	e = e->tree.right;
	if (e)
	    FilePuts (f, ", ");
    }
}

static void
PrettyChar (Value f, int c)
{
    FileOutput (f, '\'');
    if (c < ' ' || '~' < c)
	switch (c) {
	case '\n':
	    FilePuts (f, "\\n");
	    break;
	case '\r':
	    FilePuts (f, "\\r");
	    break;
	case '\b':
	    FilePuts (f, "\\b");
	    break;
	case '\t':
	    FilePuts (f, "\\t");
	    break;
	case '\f':
	    FilePuts (f, "\\f");
	    break;
	default:
	    FileOutput (f, '\\');
	    Print (f, NewInt (c), 'o', 8, 3, 0, '0');
	    break;
	}
    else if (c == '\'')
	FilePuts (f, "\\'");
    else
	FileOutput (f, c);
    FileOutput (f, '\'');
}

static void
PrettyDecl (Value f, Expr *e, int level, Bool nest, ProfileData *pd)
{
    DeclListPtr	decl;

/*    PrettyProf (f, e);*/
    FilePutPublish (f, e->decl.publish, True);
    switch (e->decl.class) {
    case class_const:
	FilePuts (f, "const");
	if (e->decl.type)
	    FilePutType (f, e->decl.type, False);
	break;
    case class_global:
	if (e->decl.type)
	    FilePutType (f, e->decl.type, False);
	else
	    FilePuts (f, "global");
	break;
    case class_auto:
	if (e->decl.type)
	    FilePutType (f, e->decl.type, False);
	else
	    FilePuts (f, "auto");
	break;
    case class_static:
    case class_typedef:
    case class_namespace:
	FilePutClass (f, e->decl.class, e->decl.type && !TypePoly (e->decl.type));
	if (e->decl.type && !TypePoly (e->decl.type))
	    FilePutType (f, e->decl.type, False);
	break;
    case class_undef:
    case class_arg:
	FilePutType (f, e->decl.type, False);
	break;
    case class_exception:
	FilePutClass (f, e->decl.class, False);
	break;
    }
    for (decl = e->decl.decl; decl; decl = decl->next)
    {
	FileOutput (f, ' ');
	FilePuts (f, AtomName (decl->name));
	if (decl->init)
	{
	    FilePuts (f, " = ");
	    PrettyExpr (f, decl->init, -1, level, nest, pd);
	}
	if (decl->next)
	    FilePuts (f, ",");
    }
    if (e->decl.class == class_exception)
    {
	FilePutArgType (f, e->decl.type->func.args);
    }
}

void
PrettyExpr (Value f, Expr *e, int parentPrec, int level, Bool nest, ProfileData *pd)
{
    int	selfPrec;

    if (!e)
	return;
    selfPrec = tokenToPrecedence (e->base.tag);
    if (selfPrec < parentPrec)
	FilePuts (f, "(");
    switch (e->base.tag) {
    case NAME:
	FilePuts (f, AtomName (e->atom.atom));
	break;
    case VAR:
	PrettyDecl (f, e, level, nest, pd);
	break;
    case OP:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	FilePuts (f, " (");
	if (e->tree.right)
	    PrettyParameters (f, e->tree.right, nest, pd);
	FilePuts (f, ")");
	break;
    case OS:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	FilePuts (f, "[");
	PrettyParameters (f, e->tree.right, nest, pd);
	FilePuts (f, "]");
	break;
    case NEW:
	FilePrintf (f, "(%T)", e->base.type);
	if (e->tree.left)
	{
	    FilePuts (f, " ");
	    PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	}
	break;
    case ARRAY:
	FilePuts (f, "{ ");
	PrettyArrayInits (f, e->tree.left, level, nest, pd);
	FilePuts (f, " }");
	break;
    case COMP:
	FilePuts (f, "{ ");
	FilePuts (f, "[");
	PrettyExpr (f, e->tree.left->tree.left, selfPrec, level, nest, pd);
	FilePuts (f, "] ");
	if (e->tree.right->base.tag == OC)
	    PrettyStatement (f, e->tree.right, level + 1, level, nest, pd);
	else
	{
	    FilePuts (f, "= ");
	    PrettyExpr (f, e->tree.right, selfPrec, level, nest, pd);
	}
	FilePuts (f, " }");
	break;
    case ANONINIT:
	FilePuts (f, "{}");
	break;
    case STRUCT:
	FilePuts (f, "{ ");
	PrettyStructInit (f, e->tree.left, level, nest, pd);
	FilePuts (f, " }");
	break;
    case UNION:
	if (e->tree.right)
	{
	    FilePrintf (f, "(%T.%A) ", e->base.type, e->tree.left->atom.atom);
	    PrettyExpr (f, e->tree.right, selfPrec, level, nest, pd);
	}
	else
	{
	    FilePrintf (f, "%T.%A", e->base.type, e->tree.left->atom.atom);
	}
	break;
    case OCTAL_NUM:
    case OCTAL0_NUM:
    case OCTAL_FLOAT:
    case OCTAL0_FLOAT:
	FilePrintf (f, "0o");
	Print (f, e->constant.constant, 'o', 8, 0, DEFAULT_OUTPUT_PRECISION, ' ');
	break;
    case BINARY_NUM:
    case BINARY_FLOAT:
	FilePrintf (f, "0b");
	Print (f, e->constant.constant, 'b', 2, 0, DEFAULT_OUTPUT_PRECISION, ' ');
	break;
    case HEX_NUM:
    case HEX_FLOAT:
	FilePrintf (f, "0x");
	Print (f, e->constant.constant, 'x', 16, 0, DEFAULT_OUTPUT_PRECISION, ' ');
	break;
    case TEN_NUM:
    case TEN_FLOAT:
    case STRING_CONST:
    case POLY_CONST:
    case THREAD_CONST:
    case VOIDVAL:
    case BOOLVAL:
	FilePrintf (f, "%v", e->constant.constant);
	break;
    case CHAR_CONST:
	PrettyChar (f, IntPart (e->constant.constant, "malformed character constant"));
	break;
    case FUNC:
	PrettyCode (f, e->code.code, 0, class_undef, publish_private, level + 1, nest);
	break;
    case POW:
    case ASSIGNPOW:
	e = e->tree.right;
	/* fall through */
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
    case ASSIGNSHIFTL:
    case ASSIGNSHIFTR:
    case ASSIGNLXOR:
    case ASSIGNLAND:
    case ASSIGNLOR:
    case COMMA:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	switch (e->base.tag) {
	case PLUS:	FilePuts (f, " + "); break;
	case MINUS:	FilePuts (f, " - "); break;
	case TIMES:	FilePuts (f, " * "); break;
	case DIVIDE:	FilePuts (f, " / "); break;
	case MOD:	FilePuts (f, " % "); break;
	case DIV:	FilePuts (f, " // "); break;
	case POW:	FilePuts (f, " ** "); break;
	case SHIFTL:	FilePuts (f, " << "); break;
	case SHIFTR:	FilePuts (f, " >> "); break;
	case LXOR:	FilePuts (f, " ^ "); break;
	case LAND:	FilePuts (f, " & "); break;
	case LOR:	FilePuts (f, " | "); break;
	case EQ:	FilePuts (f, " == "); break;
	case NE:	FilePuts (f, " != "); break;
	case LT:	FilePuts (f, " < "); break;
	case GT:	FilePuts (f, " > "); break;
	case LE:	FilePuts (f, " <= "); break;
	case GE:	FilePuts (f, " >= "); break;
	case AND:	FilePuts (f, " && "); break;
	case OR:	FilePuts (f, " || "); break;
	case ASSIGN:	FilePuts (f, " = "); break;
	case ASSIGNPLUS:FilePuts (f, " += "); break;
	case ASSIGNMINUS:FilePuts (f, " -= "); break;
	case ASSIGNTIMES:FilePuts (f, " *= "); break;
	case ASSIGNDIVIDE:FilePuts (f, " /= "); break;
	case ASSIGNMOD:	FilePuts (f, " %= "); break;
	case ASSIGNDIV:	FilePuts (f, " //= "); break;
	case ASSIGNPOW:	FilePuts (f, " **= "); break;
	case ASSIGNSHIFTL:FilePuts (f, " <<= "); break;
	case ASSIGNSHIFTR:FilePuts (f, " >>= "); break;
	case ASSIGNLXOR:FilePuts (f, " ^= "); break;
	case ASSIGNLAND:FilePuts (f, " &= "); break;
	case ASSIGNLOR:	FilePuts (f, " |= "); break;
	case COMMA:	FilePuts (f, ", "); break;
	}
	PrettyExpr (f, e->tree.right, selfPrec, level, nest, pd);
	break;
    case FACT:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	FilePuts (f, "!"); 
	break;
    case LNOT:
    case UMINUS:
    case BANG:
    case INC:
    case DEC:
	if (e->tree.right)
	    PrettyExpr (f, e->tree.right, selfPrec, level, nest, pd);
	switch (e->base.tag) {
	case LNOT:	FilePuts (f, "~"); break;
	case UMINUS:	FilePuts (f, "-"); break;
	case BANG:	FilePuts (f, "!"); break;
	case INC:	FilePuts (f, "++"); break;
	case DEC:	FilePuts (f, "--"); break;
	}
	if (e->tree.left)
	    PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	break;
    case STAR:
	FilePuts (f, "*");
	PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	break;
    case AMPER:
	FilePuts (f, "&");
	PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	break;
    case COLONCOLON:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	FilePuts (f, "::");
	FilePuts (f, AtomName (e->tree.right->atom.atom));
	break;
    case DOT:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	FileOutput (f, '.');
	FilePuts (f, AtomName (e->tree.right->atom.atom));
	break;
    case ARROW:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	FilePuts (f, "->");
	FilePuts (f, AtomName (e->tree.right->atom.atom));
	break;
    case QUEST:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	FilePuts (f, " ? ");
	PrettyExpr (f, e->tree.right->tree.left, selfPrec, level, nest, pd);
	FilePuts (f, " : ");
	PrettyExpr (f, e->tree.right->tree.right, selfPrec, level, nest, pd);
	break;
    case DOLLAR:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest, pd);
	break;
    }
    if (selfPrec < parentPrec)
	FilePuts (f, ")");
}

void
PrettyStatement (Value f, Expr *e, int level, int blevel, Bool nest, ProfileData *pd)
{
    switch (e->base.tag) {
    case EXPR:
	PrettyIndent (f, e, level, pd);
	PrettyExpr (f, e->tree.left, -1, level, nest, pd);
	FilePuts (f, ";\n");
	break;
    case IF:
	PrettyIndent (f, e, level, pd);
	FilePuts (f, "if (");
	PrettyExpr (f, e->tree.left, -1, level, nest, pd);
	FilePuts (f, ")\n");
	if (nest)
	    PrettyStatement (f, e->tree.right, level+1, level, nest, pd);
	break;
    case ELSE:
	PrettyIndent (f, e, level, pd);
	FilePuts (f, "if (");
	PrettyExpr (f, e->tree.left, -1, level, nest, pd);
	FilePuts (f, ")\n");
	if (nest)
	{
	    PrettyStatement (f, e->tree.right->tree.left, level+1, level, nest, pd);
	    PrettyIndent (f, 0, level, pd);
	    FilePuts (f, "else\n");
	    PrettyStatement (f, e->tree.right->tree.right, level+1, level, nest, pd);
	}
	break;
    case WHILE:
	PrettyIndent (f, e, level, pd);
	FilePuts (f, "while (");
	PrettyExpr (f, e->tree.left, -1, level, nest, pd);
	FilePuts (f, ")\n");
	if (nest)
	    PrettyStatement (f, e->tree.right, level+1, level, nest, pd);
	break;
    case OC:
	PrettyIndent (f, 0, blevel, pd);
	FilePuts (f, "{\n");
	PrettyBlock (f, e, blevel + 1, nest, pd);
	PrettyIndent (f, 0, blevel, pd);
	FilePuts (f, "}\n");
	break;
    case DO:
	PrettyIndent (f, 0, level, pd);
	FilePuts (f, "do\n");
	if (nest)
	    PrettyStatement (f, e->tree.left, level+1, level, nest, pd);
	PrettyIndent (f, e, level, pd);
	FilePuts (f, "while (");
	PrettyExpr (f, e->tree.right, -1, level, nest, pd);
	FilePuts (f, ");\n");
	break;
    case FOR:
	PrettyIndent (f, e, level, pd);
	FilePuts (f, "for (");
	PrettyExpr (f, e->tree.left->tree.left, -1, level, nest, pd);
	FilePuts (f, ";");
	if (e->tree.left->tree.right)
	{
	    FilePuts (f, " ");
	    PrettyExpr (f, e->tree.left->tree.right, -1, level, nest, pd);
	}
	FilePuts (f, ";");
	if (e->tree.right->tree.left)
	{
	    FilePuts (f, " ");
	    PrettyExpr (f, e->tree.right->tree.left, -1, level, nest, pd);
	}
	FilePuts (f, ")\n");
	if (nest)
	    PrettyStatement (f, e->tree.right->tree.right, level+1, level, nest, pd);
	break;
    case SWITCH:
    case UNION:
	PrettyIndent (f, e, level, pd);
	if (e->base.tag == SWITCH)
	    FilePuts (f, "switch (");
	else
	    FilePuts (f, "union switch (");
	PrettyExpr (f, e->tree.left, -1, level, nest, pd);
	FilePuts (f, ")");
	if (nest)
	{
	    ExprPtr block = e->tree.right;

	    FilePuts (f, " {\n");	    
	    while (block)
	    {
		PrettyIndent (f, 0, level, pd);
		if (block->tree.left->tree.left)
		{
		    FilePuts (f, "case ");
		    PrettyExpr (f, block->tree.left->tree.left, -1, level, nest, pd);
		}
		else
		    FilePuts (f, "default");
		FilePuts (f, ":\n");
		PrettyBlock (f, block->tree.left->tree.right, level+1, nest, pd);
		block = block->tree.right;
	    }
	    PrettyIndent (f, 0, level, pd);
	    FilePuts (f, "}");
	}
        FilePuts (f, "\n");
	break;
    case SEMI:
	PrettyIndent (f, e, level, pd);
	FilePuts (f, ";\n");
	break;
    case BREAK:
	PrettyIndent (f, e, level, pd);
	FilePuts (f, "break;\n");
	break;
    case CONTINUE:
	PrettyIndent (f, e, level, pd);
	FilePuts (f, "continue;\n");
	break;
    case RETURNTOK:
	PrettyIndent (f, e, level, pd);
	FilePuts (f, "return ");
	PrettyExpr (f, e->tree.right, -1, level, nest, pd);
	FilePuts (f, ";\n");
	break;
    case FUNC:
	PrettyIndent (f, e, level, pd);
	{
	    DeclListPtr	    decl = e->decl.decl;
	    ExprPtr	    init = decl->init;
	    CodePtr	    code = init->code.code;

	    PrettyCode (f, code, decl->name, 
			e->decl.class,
			e->decl.publish,
			level, nest);
	}
	FilePuts (f, "\n");
	break;
    case TYPEDEF:
	e = e->tree.left;
	/* fall through */
    case VAR:
	PrettyIndent (f, e, level, pd);
	PrettyDecl (f, e, level, nest, pd);
        FilePuts (f, ";\n");
	break;
    case NAMESPACE:
	PrettyIndent (f, e, level, pd);
	FilePuts (f, "namespace ");
	PrettyExpr (f, e->tree.left, -1, level, nest, pd);
	FilePuts (f, "\n");
	PrettyStatement (f, e->tree.right, level + 1, level, nest, pd);
	break;
    case IMPORT:
	PrettyIndent (f, e, level, pd);
	FilePrintf (f, "%pimport %A;\n", e->tree.left->decl.publish,
		    e->tree.left->decl.decl->name);
	break;
    case TWIXT:
	PrettyIndent (f, e, level, pd);
	FilePuts (f, "twixt (");
	PrettyExpr (f, e->tree.left->tree.left, -1, level, nest, pd);
	FilePuts (f, "; ");
	PrettyExpr (f, e->tree.left->tree.right, -1, level, nest, pd);
	FilePuts (f, ")\n");
	if (nest)
	    PrettyStatement (f, e->tree.right->tree.left, level+1, level, nest, pd);
	break;
    case CATCH:
	PrettyIndent (f, e, level, pd);
	FilePuts (f, "try");
	if (nest)
	{
	    FilePuts (f, "\n");
	    PrettyStatement (f, e->tree.left, level+1, level, nest, pd);
	}
	else
	    FilePuts (f, " ");
	while ((e = e->tree.right))
	{
	    CodePtr	catch;
	    Atom	name;
	    if (nest)
	    {
		PrettyIndent (f, 0, level, pd);
	    }
	    catch = e->tree.left->code.code;
	    if (catch->base.name->base.tag == COLONCOLON)
		name = catch->base.name->tree.right->atom.atom;
	    else
		name = catch->base.name->atom.atom;
	    FilePuts (f, "catch ");
	    PrettyExpr (f, catch->base.name, 0, level, nest, pd);
	    FilePuts (f, " ");
	    PrettyBody (f, e->tree.left->code.code, level, nest, pd);
	    FilePuts (f, "\n");
	}
	break;
    case RAISE:
	PrettyIndent (f, e, level, pd);
	FilePrintf (f, "raise %A (", e->tree.left->atom.atom);
	if (e->tree.right)
	    PrettyParameters (f, e->tree.right, nest, pd);
	FilePuts (f, ");\n");
	break;
    case DOLLAR:
	PrettyIndent (f, e, level, pd);
	PrettyExpr (f, e->tree.left, -1, level, nest, pd);
	FilePuts (f, "\n");
	break;
    }
}

static void
PrintArgs (Value f, ArgType *args)
{
    FilePuts (f, "(");
    for (; args; args = args->next)
    {
        FilePutType (f, args->type, args->name != 0);
	if (args->name)
	    FilePuts (f, AtomName (args->name));
	if (args->varargs)
	    FilePuts (f, " ...");
	if (args->next)
	    FilePuts (f, ", ");
    }
    FilePuts (f, ")");
}

static void
PrettyBody (Value f, CodePtr code, int level, Bool nest, ProfileData *pd)
{
    PrintArgs (f, code->base.args);
    if (code->base.builtin)
    {
	FilePuts (f, " <builtin>");
    }
    else
    {
	if (nest)
	{
	    FilePuts (f, "\n");
	    PrettyIndent (f, 0, level, pd);
	    FilePuts (f, "{\n");
	    PrettyBlock (f, code->func.code, level + 1, nest, pd);
	    PrettyIndent (f, 0, level, pd);
	    FilePuts (f, "}");
	}
	else
	    FilePuts (f, ";");
    }
}

void
PrettyCode (Value f, CodePtr code, Atom name, Class class, Publish publish,
	   int level, Bool nest)
{
    ProfileData	pd;
    pd.sub = pd.self = 0;
    if (name)
	FilePrintf (f, "%p%k%T %A ", publish, class, code->base.type, name);
    else
	FilePrintf (f, "%tfunc", code->base.type);
    PrettyBody (f, code, level, nest, &pd);
    if (!code->base.builtin && nest && profiling)
    {
	FilePuts (f, "\n---------------------\n");
	PrettyProfNum (f, pd.sub, 1);
	FilePuts (f, " ");
	PrettyProfNum (f, pd.self, 1);
	if (name)
	    FilePrintf (f, ": %A\n", name);
	else
	    FilePrintf (f, ": %tfunc\n", code->base.type);
    }
}

void
PrettyStat (Value f, Expr *e, Bool nest)
{
    PrettyStatement (f, e, 1, 1, nest, 0);
}

void
doPrettyPrint (Value f, Publish publish, SymbolPtr symbol, int level, Bool nest);
    
static void
PrintNames (Value f, NamelistPtr names, int level)
{
    if (!names)
	return;
    PrintNames (f, names->next, level);
    if (names->publish != publish_private)
	doPrettyPrint (f, names->publish, names->symbol, level, False);
}

static void
PrintNamespace (Value f, NamespacePtr namespace, int level)
{
    PrintNames (f, namespace->names, level);
}

void
doPrettyPrint (Value f, Publish publish, SymbolPtr symbol, int level, Bool nest)
{
    Value  v;

    if (!symbol)
	return;
    if (profiling)
	FilePuts (f, "    called       self\n");
    PrettyIndent (f, 0, level, 0);
    switch (symbol->symbol.class) {
    case class_const:
    case class_global:
	v = BoxValueGet (symbol->global.value, 0);
	if (v && ValueIsFunc(v))
	{
	    PrettyCode (f, v->func.code,
		       symbol->symbol.name,
		       class_undef,
		       publish,
		       level, nest);
	}
	else
	{
	    FilePrintf (f, "%p%k%t%A = %v;",
			publish, symbol->symbol.class, symbol->symbol.type,
			symbol->symbol.name, v);
	}
	FilePuts (f, "\n");
	break;
    case class_namespace:
	FilePrintf (f, "%p%C %A {\n", publish, symbol->symbol.class, symbol->symbol.name);
	PrintNamespace (f, symbol->namespace.namespace, level + 1);
	PrettyIndent (f, 0, level, 0);
	FilePuts (f, "}\n");
	break;
    case class_exception:
	FilePrintf (f, "%p%C %A ", publish, 
		    symbol->symbol.class, symbol->symbol.name);
	PrintArgs (f, symbol->symbol.type->func.args);
	FilePuts (f, ";\n");
	break;
    default:
	FilePrintf (f, "%p%k%t%A;\n",
		    publish, symbol->symbol.class,
		    symbol->symbol.type, symbol->symbol.name);
	break;
    }
}

void
PrettyPrint (Value f, Publish publish, SymbolPtr name)
{
    doPrettyPrint (f, publish, name, 0, True);
}

