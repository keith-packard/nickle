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

extern Bool profiling;

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
    int	spaces = 7 - PrettyNumWidth (i);
    if (pad_left)
	while (spaces--)
	    FilePuts (f, " ");
    FilePrintf (f, "%d", i);
    if (!pad_left)
	while (spaces--)
	    FilePuts (f, " ");
}

static void
PrettyProf (Value f, Expr *e)
{
    if (profiling)
    {
	PrettyProfNum (f, e ? e->base.sub_ticks : 0, 1);
	FilePuts (f, ",");
	PrettyProfNum (f, e ? e->base.ticks : 0, 0);
	FilePuts (f, ": ");
    }
}
	    
static void PrettyParameters (Value f, Expr *e, Bool nest);
static void PrettyArrayInit (Value f, Expr *e, int level, Bool nest);
static void PrettyStatement (Value f, Expr *e, int level, int blevel, Bool nest);

static void
PrettyIndent (Value f, int level)
{
    int	i;
    for (i = 0; i < level-1; i += 2)
	FileOutput (f, '\t');
    if (i < level)
	FilePuts (f, "    ");
}

static void
PrettyBlock (Value f, Expr *e, int level, Bool nest)
{
    while (e->tree.left) {
	PrettyStatement (f, e->tree.left, level, level, nest);
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
PrettyParameters (Value f, Expr *e, Bool nest)
{
    while (e) 
    {
	PrettyExpr (f, e->tree.left, -1, 0, nest);
	e = e->tree.right;
	if (e)
	    FilePuts (f, ", ");
    }
}

static void
PrettyArrayInit (Value f, Expr *e, int level, Bool nest);

static void
PrettyArrayInits (Value f, Expr *e, int level, Bool nest)
{
    while (e)
    {
	PrettyArrayInit (f, e->tree.left, 0, nest);
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
PrettyArrayInit (Value f, Expr *e, int level, Bool nest)
{
    switch (e->base.tag) {
    case OC:
	FilePuts (f, "{ ");
	PrettyArrayInits (f, e->tree.left, level, nest);
	FilePuts (f, " }");
	break;
    case DOTS:
	FilePuts (f, "...");
	break;
    default:
	PrettyExpr (f, e, -1, level, nest);
	break;
    }
}

static void
PrettyStructInit (Value f, Expr *e, int level, Bool nest)
{
    while (e)
    {
	FilePuts (f, AtomName (e->tree.left->tree.left->atom.atom));
	FilePuts (f, " = ");
	PrettyExpr (f, e->tree.left->tree.right, -1, level, nest);
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
PrettyDecl (Value f, Expr *e, int level, Bool nest)
{
    DeclListPtr	decl;

/*    PrettyProf (f, e);*/
    FilePutPublish (f, e->decl.publish, True);
    switch (e->decl.class) {
    case class_const:
	FilePuts (f, "const");
	if (e->decl.type)
	    FilePutTypes (f, e->decl.type, False);
	break;
    case class_global:
	if (e->decl.type)
	    FilePutTypes (f, e->decl.type, False);
	else
	    FilePuts (f, "global");
	break;
    case class_auto:
	if (e->decl.type)
	    FilePutTypes (f, e->decl.type, False);
	else
	    FilePuts (f, "auto");
	break;
    case class_static:
    case class_typedef:
    case class_namespace:
	FilePutClass (f, e->decl.class, !TypePoly (e->decl.type));
	if (!TypePoly (e->decl.type))
	    FilePutTypes (f, e->decl.type, False);
	break;
    case class_undef:
    case class_arg:
	FilePutTypes (f, e->decl.type, False);
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
	    PrettyExpr (f, decl->init, -1, level, nest);
	}
	if (decl->next)
	    FilePuts (f, ",");
    }
    if (e->decl.class == class_exception)
    {
	FilePutArgTypes (f, e->decl.type->func.args);
    }
}

void
PrettyExpr (Value f, Expr *e, int parentPrec, int level, Bool nest)
{
    int	selfPrec;

    if (!e)
	return;
/*    PrettyProf (f, e); */
    selfPrec = tokenToPrecedence (e->base.tag);
    if (selfPrec < parentPrec)
	FilePuts (f, "(");
    switch (e->base.tag) {
    case NAME:
	FilePuts (f, AtomName (e->atom.atom));
	break;
    case VAR:
	PrettyDecl (f, e, level, nest);
	break;
    case OP:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, " (");
	if (e->tree.right)
	    PrettyParameters (f, e->tree.right, nest);
	FilePuts (f, ")");
	break;
    case OS:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, "[");
	PrettyParameters (f, e->tree.right, nest);
	FilePuts (f, "]");
	break;
    case NEW:
	FilePrintf (f, "(%T)", e->base.type);
	if (e->tree.left)
	{
	    FilePuts (f, " ");
	    PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	}
	break;
    case ARRAY:
	FilePuts (f, "{ ");
	PrettyArrayInits (f, e->tree.left, level, nest);
	FilePuts (f, " }");
	break;
    case STRUCT:
	FilePuts (f, "{ ");
	PrettyStructInit (f, e->tree.left, level, nest);
	FilePuts (f, " }");
	break;
    case UNION:
	FilePrintf (f, "%T.%A ", e->base.type, e->tree.left->atom.atom);
	if (e->tree.right)
	{
	    FilePuts (f, "(");
	    PrettyExpr (f, e->tree.right, selfPrec, level, nest);
	    FilePuts (f, ")");
	}
	break;
    case TEN_CONST:
    case OCTAL_CONST:
    case BINARY_CONST:
    case HEX_CONST:
    case FLOAT_CONST:
    case STRING_CONST:
    case POLY_CONST:
    case THREAD_CONST:
    case VOIDVAL:
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
	PrettyExpr (f, e->tree.left, selfPrec, level, nest);
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
	PrettyExpr (f, e->tree.right, selfPrec, level, nest);
	break;
    case FACT:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, "!"); 
	break;
    case LNOT:
    case UMINUS:
    case BANG:
    case INC:
    case DEC:
	if (e->tree.right)
	    PrettyExpr (f, e->tree.right, selfPrec, level, nest);
	switch (e->base.tag) {
	case LNOT:	FilePuts (f, "~"); break;
	case UMINUS:	FilePuts (f, "-"); break;
	case BANG:	FilePuts (f, "!"); break;
	case INC:	FilePuts (f, "++"); break;
	case DEC:	FilePuts (f, "--"); break;
	}
	if (e->tree.left)
	    PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	break;
    case STAR:
	FilePuts (f, "*");
	PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	break;
    case AMPER:
	FilePuts (f, "&");
	PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	break;
    case COLONCOLON:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, "::");
	FilePuts (f, AtomName (e->tree.right->atom.atom));
	break;
    case DOT:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	FileOutput (f, '.');
	FilePuts (f, AtomName (e->tree.right->atom.atom));
	break;
    case ARROW:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, "->");
	FilePuts (f, AtomName (e->tree.right->atom.atom));
	break;
    case QUEST:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, " ? ");
	PrettyExpr (f, e->tree.right->tree.left, selfPrec, level, nest);
	FilePuts (f, " : ");
	PrettyExpr (f, e->tree.right->tree.right, selfPrec, level, nest);
	break;
    case DOLLAR:
	PrettyExpr (f, e->tree.left, selfPrec, level, nest);
	break;
    }
    if (selfPrec < parentPrec)
	FilePuts (f, ")");
}

void
PrettyStatement (Value f, Expr *e, int level, int blevel, Bool nest)
{
    PrettyProf (f, e);
    switch (e->base.tag) {
    case EXPR:
	PrettyIndent (f, level);
	PrettyExpr (f, e->tree.left, -1, level, nest);
	FilePuts (f, ";\n");
	break;
    case IF:
	PrettyIndent (f, level);
	FilePuts (f, "if (");
	PrettyExpr (f, e->tree.left, -1, level, nest);
	FilePuts (f, ")\n");
	if (nest)
	    PrettyStatement (f, e->tree.right, level+1, level, nest);
	break;
    case ELSE:
	PrettyIndent (f, level);
	FilePuts (f, "if (");
	PrettyExpr (f, e->tree.left, -1, level, nest);
	FilePuts (f, ")\n");
	if (nest)
	{
	    PrettyStatement (f, e->tree.right->tree.left, level+1, level, nest);
	    PrettyProf (f, 0);
	    PrettyIndent (f, level);
	    FilePuts (f, "else\n");
	    PrettyStatement (f, e->tree.right->tree.right, level+1, level, nest);
	}
	break;
    case WHILE:
	PrettyIndent (f, level);
	FilePuts (f, "while (");
	PrettyExpr (f, e->tree.left, -1, level, nest);
	FilePuts (f, ")\n");
	if (nest)
	    PrettyStatement (f, e->tree.right, level+1, level, nest);
	break;
    case OC:
	PrettyIndent (f, blevel);
	FilePuts (f, "{\n");
	PrettyBlock (f, e, blevel + 1, nest);
	PrettyProf (f, 0);
	PrettyIndent (f, blevel);
	FilePuts (f, "}\n");
	break;
    case DO:
	PrettyIndent (f, level);
	FilePuts (f, "do\n");
	if (nest)
	    PrettyStatement (f, e->tree.left, level+1, level, nest);
	PrettyIndent (f, level);
	FilePuts (f, "while (");
	PrettyExpr (f, e->tree.right, -1, level, nest);
	FilePuts (f, ");\n");
	break;
    case FOR:
	PrettyIndent (f, level);
	FilePuts (f, "for (");
	PrettyExpr (f, e->tree.left->tree.left, -1, level, nest);
	FilePuts (f, ";");
	if (e->tree.left->tree.right)
	{
	    FilePuts (f, " ");
	    PrettyExpr (f, e->tree.left->tree.right, -1, level, nest);
	}
	FilePuts (f, ";");
	if (e->tree.right->tree.left)
	{
	    FilePuts (f, " ");
	    PrettyExpr (f, e->tree.right->tree.left, -1, level, nest);
	}
	FilePuts (f, ")\n");
	if (nest)
	    PrettyStatement (f, e->tree.right->tree.right, level+1, level, nest);
	break;
    case SWITCH:
    case UNION:
	PrettyIndent (f, level);
	if (e->base.tag == SWITCH)
	    FilePuts (f, "switch (");
	else
	    FilePuts (f, "union switch (");
	PrettyExpr (f, e->tree.left, -1, level, nest);
	FilePuts (f, ")");
	if (nest)
	{
	    ExprPtr block = e->tree.right;

	    FilePuts (f, " {\n");	    
	    while (block)
	    {
		PrettyIndent (f, level);
		if (block->tree.left->tree.left)
		{
		    FilePuts (f, "case ");
		    PrettyExpr (f, block->tree.left->tree.left, -1, level, nest);
		}
		else
		    FilePuts (f, "default");
		FilePuts (f, ":\n");
		PrettyBlock (f, block->tree.left->tree.right, level+1, nest);
		block = block->tree.right;
	    }
	    PrettyIndent (f, level);
	    FilePuts (f, "}");
	}
        FilePuts (f, "\n");
	break;
    case SEMI:
	PrettyIndent (f, level);
	FilePuts (f, ";\n");
	break;
    case BREAK:
	PrettyIndent (f, level);
	FilePuts (f, "break;\n");
	break;
    case CONTINUE:
	PrettyIndent (f, level);
	FilePuts (f, "continue;\n");
	break;
    case RETURNTOK:
	PrettyIndent (f, level);
	FilePuts (f, "return ");
	PrettyExpr (f, e->tree.right, -1, level, nest);
	FilePuts (f, ";\n");
	break;
    case FUNC:
	PrettyIndent (f, level);
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
	PrettyIndent (f, level);
	PrettyDecl (f, e, level, nest);
        FilePuts (f, ";\n");
	break;
    case NAMESPACE:
	PrettyIndent (f, level);
	FilePuts (f, "namespace ");
	PrettyExpr (f, e->tree.left, -1, level, nest);
	FilePuts (f, "\n");
	PrettyStatement (f, e->tree.right, level + 1, level, nest);
	break;
    case IMPORT:
	PrettyIndent (f, level);
	FilePrintf (f, "%pimport %A;\n", e->tree.left->decl.publish,
		    e->tree.left->decl.decl->name);
	break;
    case TWIXT:
	PrettyIndent (f, level);
	FilePuts (f, "twixt (");
	PrettyExpr (f, e->tree.left->tree.left, -1, level, nest);
	FilePuts (f, "; ");
	PrettyExpr (f, e->tree.left->tree.right, -1, level, nest);
	FilePuts (f, ")\n");
	if (nest)
	{
	    PrettyStatement (f, e->tree.right->tree.left, level+1, level, nest);
	    if (e->tree.right->tree.right)
	    {
		PrettyProf (f, 0);
		PrettyIndent (f, level);
		FilePuts (f, "else\n");
		PrettyStatement (f, e->tree.right->tree.right, level+1, level, nest);
	    }
	}
	break;
    case CATCH:
	PrettyIndent (f, level);
	FilePuts (f, "try");
	if (nest)
	{
	    FilePuts (f, "\n");
	    PrettyStatement (f, e->tree.left, level+1, level, nest);
	}
	else
	    FilePuts (f, " ");
	while ((e = e->tree.right))
	{
	    if (nest)
	    {
		PrettyProf (f, 0);
		PrettyIndent (f, level);
	    }
	    FilePrintf (f, "catch %A ", e->tree.left->code.code->base.name->atom.atom);
	    PrettyCode (f, e->tree.left->code.code,
		       0, class_undef,
		       publish_private, level, nest);
	    if (!nest)
		FilePuts (f, "\n");
	}
	break;
    case RAISE:
	PrettyIndent (f, level);
	FilePrintf (f, "raise %A (", e->tree.left->atom.atom);
	if (e->tree.right)
	    PrettyParameters (f, e->tree.right, nest);
	FilePuts (f, ");\n");
	break;
    case DOLLAR:
	PrettyIndent (f, level);
	PrettyExpr (f, e->tree.left, -1, level, nest);
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
        FilePutTypes (f, args->type, args->name != 0);
	if (args->name)
	    FilePuts (f, AtomName (args->name));
	if (args->varargs)
	    FilePuts (f, " ...");
	if (args->next)
	    FilePuts (f, ", ");
    }
    FilePuts (f, ")");
}

void
PrettyCode (Value f, CodePtr code, Atom name, Class class, Publish publish,
	   int level, Bool nest)
{
    if (name)
	FilePrintf (f, "%p%k%T %A ", publish, class, code->base.type, name);
    else
	FilePrintf (f, "%tfunc", code->base.type);
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
	    PrettyIndent (f, level);
	    FilePuts (f, "{\n");
	    PrettyBlock (f, code->func.code, level + 1, nest);
	    PrettyIndent (f, level);
	    FilePuts (f, "}");
	}
	else
	    FilePuts (f, ";");
    }
}

void
PrettyStat (Value f, Expr *e, Bool nest)
{
    PrettyStatement (f, e, 1, 1, nest);
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
    PrettyIndent (f, level);
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
	PrettyIndent (f, level);
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

