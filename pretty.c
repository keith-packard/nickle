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

#include	<config.h>

#include	<stdio.h>
#include	"nickle.h"
#include	"gram.h"

void printParameters (Value f, Expr *e, Bool nest);
void printAinit (Value f, Expr *e, int level, Bool nest);
void printExpr (Value f, Expr *e, int parentPrec, int level, Bool nest);
void printStatement (Value f, Expr *e, int level, int blevel, Bool nest);
void printTypes (Value f, Types *t);
    
static void
printclass (Value f, Class class)
{
    switch (class) {
    case class_global:
	FilePuts (f, "global");
	break;
    case class_static:
	FilePuts (f, "static");
	break;
    case class_arg:
	FilePuts (f, "arg");
	break;
    case class_auto:
	FilePuts (f, "auto");
	break;
    case class_typedef:
	FilePuts (f, "typedef");
	break;
    case class_namespace:
	FilePuts (f, "namespace");
	break;
    case class_exception:
	FilePuts (f, "exception");
	break;
    case class_undef:
	break;
    }
}

static void
printpublish (Value f, Publish publish)
{
    switch (publish) {
    case publish_public:
	FilePuts (f, "public ");
	break;
    case publish_private:
	break;
    }
}

static void
printindent (Value f, int level)
{
    int	i;
    for (i = 0; i < level-1; i += 2)
	FileOutput (f, '\t');
    if (i < level)
	FilePuts (f, "    ");
}

static void
printBlock (Value f, Expr *e, int level, Bool nest)
{
    while (e->tree.left) {
	printStatement (f, e->tree.left, level, level, nest);
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

void
printParameters (Value f, Expr *e, Bool nest)
{
    if (e->tree.right) {
	printParameters (f, e->tree.right, nest);
	FilePuts (f, ", ");
    }
    printExpr (f, e->tree.left, -1, 0, nest);
}

static void
printAinits (Value f, Expr *e, int level, Bool nest)
{
    printAinit (f, e->tree.left, 0, nest);
    if (e->tree.right) {
	FilePuts (f, ", ");
	printAinits (f, e->tree.right, level, nest);
    }
}

void
printAinit (Value f, Expr *e, int level, Bool nest)
{
    switch (e->base.tag) {
    case OC:
	FilePuts (f, "{ ");
	printAinits (f, e->tree.left, level, nest);
	FilePuts (f, " }");
	break;
    default:
	printExpr (f, e, -1, level, nest);
	break;
    }
}

#if 0
static void
printSinit (Value f, Expr *e, int level, Bool nest)
{
    while (e)
    {
	FilePuts (f, AtomName (e->tree.left->tree.left->atom.atom));
	FilePuts (f, " = ");
	printExpr (f, e->tree.left->tree.right, -1, level, nest);
	e = e->tree.right;
	if (e)
	    FilePuts (f, ", ");
    }
}
#endif

void
printExpr (Value f, Expr *e, int parentPrec, int level, Bool nest)
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
    case OP:
	printExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, " (");
	if (e->tree.right)
	    printParameters (f, e->tree.right, nest);
	FilePuts (f, ")");
	break;
#if 0
    case ARRAY:
	FilePuts (f, "[");
	if (e->tree.left)
	    printParameters (f, e->tree.left, nest);
	FilePuts (f, "]");
	if (e->tree.right)
	    printAinit (f, e->tree.right, level, nest);
	break;
    case OS:
	printExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, " [");
	printParameters (f, e->tree.right, nest);
	FilePuts (f, "] ");
	break;
    case STRUCT:
	printExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, " {");
	printSinit (f, e->tree.right, level, nest);
	FilePuts (f, " }");
	break;
#endif
    case CONST:
	print (f, e->constant.constant);
	break;
    case FUNCTION:
	PrintCode (f, e->code.code, 0, class_undef, publish_private, level + 1, nest);
	break;
    case PLUS:
    case MINUS:
    case TIMES:
    case DIVIDE:
    case MOD:
    case DIV:
    case POW:
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
    case ASSIGNPOW:
    case ASSIGNSHIFTL:
    case ASSIGNSHIFTR:
    case ASSIGNLXOR:
    case ASSIGNLAND:
    case ASSIGNLOR:
    case COMMA:
	printExpr (f, e->tree.left, selfPrec, level, nest);
	switch (e->base.tag) {
	case PLUS:	FilePuts (f, " + "); break;
	case MINUS:	FilePuts (f, " - "); break;
	case TIMES:	FilePuts (f, " * "); break;
	case DIVIDE:	FilePuts (f, " / "); break;
	case MOD:	FilePuts (f, " % "); break;
	case DIV:	FilePuts (f, " // "); break;
	case POW:	FilePuts (f, " ^ "); break;
	case SHIFTL:	FilePuts (f, " << "); break;
	case SHIFTR:	FilePuts (f, " >> "); break;
	case LXOR:	FilePuts (f, " ^^ "); break;
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
	case ASSIGNPOW:	FilePuts (f, " ^= "); break;
	case ASSIGNSHIFTL:FilePuts (f, " <<= "); break;
	case ASSIGNSHIFTR:FilePuts (f, " >>= "); break;
	case ASSIGNLXOR:FilePuts (f, " ^^= "); break;
	case ASSIGNLAND:FilePuts (f, " &= "); break;
	case ASSIGNLOR:	FilePuts (f, " |= "); break;
	case COMMA:	FilePuts (f, ", "); break;
	}
	printExpr (f, e->tree.right, selfPrec, level, nest);
	break;
    case FACT:
	printExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, "!"); 
	break;
    case LNOT:
    case UMINUS:
    case BANG:
    case INC:
    case DEC:
	if (e->tree.right)
	    printExpr (f, e->tree.right, selfPrec, level, nest);
	switch (e->base.tag) {
	case LNOT:	FilePuts (f, "~"); break;
	case UMINUS:	FilePuts (f, "-"); break;
	case BANG:	FilePuts (f, "!"); break;
	case INC:	FilePuts (f, "++"); break;
	case DEC:	FilePuts (f, "--"); break;
	}
	if (e->tree.left)
	    printExpr (f, e->tree.left, selfPrec, level, nest);
	break;
    case STAR:
	FilePuts (f, "*");
	printExpr (f, e->tree.left, selfPrec, level, nest);
	break;
    case AMPER:
	FilePuts (f, "&");
	printExpr (f, e->tree.left, selfPrec, level, nest);
	break;
    case COLON:
	printExpr (f, e->tree.left, selfPrec, level, nest);
	FileOutput (f, ':');
	FilePuts (f, AtomName (e->tree.right->atom.atom));
	break;
    case DOT:
	printExpr (f, e->tree.left, selfPrec, level, nest);
	FileOutput (f, '.');
	FilePuts (f, AtomName (e->tree.right->atom.atom));
	break;
    case ARROW:
	printExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, "->");
	FilePuts (f, AtomName (e->tree.right->atom.atom));
	break;
    case QUEST:
	printExpr (f, e->tree.left, selfPrec, level, nest);
	FilePuts (f, " ? ");
	printExpr (f, e->tree.right->tree.left, selfPrec, level, nest);
	FilePuts (f, " : ");
	printExpr (f, e->tree.right->tree.right, selfPrec, level, nest);
	break;
    }
    if (selfPrec < parentPrec)
	FilePuts (f, ")");
}

static void
printDecl (Value f, Expr *e, int level, Bool nest)
{
    DeclListPtr	decl;

    printpublish (f, e->decl.publish);
    switch (e->decl.class) {
    case class_global:
	if (e->decl.type)
	    fprintTypes (f, e->decl.type);
	else
	    FilePuts (f, "global");
	break;
    case class_auto:
	if (e->decl.type)
	    fprintTypes (f, e->decl.type);
	else
	    FilePuts (f, "auto");
	break;
    case class_static:
    case class_typedef:
	printclass (f, e->decl.class);
	if (!TypePoly (e->decl.type))
	{
	    FilePuts (f, " ");
	    fprintTypes (f, e->decl.type);
	}
	break;
    case class_undef:
    default:
	fprintTypes (f, e->decl.type);
	break;
    }
    for (decl = e->decl.decl; decl; decl = decl->next)
    {
	FileOutput (f, ' ');
	FilePuts (f, AtomName (decl->name));
	if (decl->init)
	{
	    FilePuts (f, " = ");
	    printExpr (f, decl->init, -1, level, nest);
	}
	if (decl->next)
	    FilePuts (f, ", ");
	else
	    FilePuts (f, ";\n");
    }
}

void
printStatement (Value f, Expr *e, int level, int blevel, Bool nest)
{
    switch (e->base.tag) {
    case EXPR:
	printindent (f, level);
	printExpr (f, e->tree.left, -1, level, nest);
	FilePuts (f, ";\n");
	break;
    case IF:
	printindent (f, level);
	FilePuts (f, "if (");
	printExpr (f, e->tree.left, -1, level, nest);
	FilePuts (f, ")\n");
	if (nest)
	    printStatement (f, e->tree.right, level+1, level, nest);
	break;
    case ELSE:
	printindent (f, level);
	FilePuts (f, "if (");
	printExpr (f, e->tree.left, -1, level, nest);
	FilePuts (f, ")\n");
	if (nest)
	{
	    printStatement (f, e->tree.right->tree.left, level+1, level, nest);
	    printindent (f, level);
	    FilePuts (f, "else\n");
	    printStatement (f, e->tree.right->tree.right, level+1, level, nest);
	}
	break;
    case WHILE:
	printindent (f, level);
	FilePuts (f, "while (");
	printExpr (f, e->tree.left, -1, level, nest);
	FilePuts (f, ")\n");
	if (nest)
	    printStatement (f, e->tree.right, level+1, level, nest);
	break;
    case OC:
	printindent (f, blevel);
	FilePuts (f, "{\n");
	printBlock (f, e, blevel + 1, nest);
	printindent (f, blevel);
	FilePuts (f, "}\n");
	break;
    case DO:
	printindent (f, level);
	FilePuts (f, "do\n");
	if (nest)
	    printStatement (f, e->tree.left, level+1, level, nest);
	printindent (f, level);
	FilePuts (f, "while (");
	printExpr (f, e->tree.right, -1, level, nest);
	FilePuts (f, ");\n");
	break;
    case FOR:
	printindent (f, level);
	FilePuts (f, "for (");
	printExpr (f, e->tree.left->tree.left, -1, level, nest);
	FilePuts (f, "; ");
	printExpr (f, e->tree.left->tree.right, -1, level, nest);
	FilePuts (f, "; ");
	printExpr (f, e->tree.right->tree.left, -1, level, nest);
	FilePuts (f, ")\n");
	if (nest)
	    printStatement (f, e->tree.right->tree.right, level+1, level, nest);
	break;
    case SEMI:
	printindent (f, level);
	FilePuts (f, ";\n");
	break;
    case BREAK:
	printindent (f, level);
	FilePuts (f, "break;\n");
	break;
    case CONTINUE:
	printindent (f, level);
	FilePuts (f, "continue;\n");
	break;
    case RETURNTOK:
	printindent (f, level);
	FilePuts (f, "return ");
	printExpr (f, e->tree.right, -1, level, nest);
	FilePuts (f, ";\n");
	break;
    case FUNCTION:
	printindent (f, level);
	PrintCode (f, e->tree.right->tree.right->code.code, 
		   e->tree.left->decl.decl->name,
		   e->tree.left->decl.class,
		   e->tree.left->decl.publish,
		   level, nest);
	FilePuts (f, "\n");
	break;
    case VAR:
	printindent (f, level);
	printDecl (f, e, level, nest);
	break;
    case NAMESPACE:
	printindent (f, level);
	FilePuts (f, "namespace ");
	printExpr (f, e->tree.left, -1, level, nest);
	FilePuts (f, "\n");
	printStatement (f, e->tree.right, level + 1, level, nest);
	break;
    case IMPORT:
	printindent (f, level);
	FilePuts (f, "import ");
	printpublish (f, e->tree.left->decl.publish);
	FilePuts (f, AtomName (e->tree.left->decl.decl->name));
	FilePuts (f, ";\n");
	break;
    case TWIXT:
	printindent (f, level);
	FilePuts (f, "twixt (");
	printExpr (f, e->tree.left->tree.left, -1, level, nest);
	FilePuts (f, "; ");
	printExpr (f, e->tree.left->tree.right, -1, level, nest);
	FilePuts (f, ")\n");
	if (nest)
	{
	    printStatement (f, e->tree.right->tree.left, level+1, level, nest);
	    if (e->tree.right->tree.right)
	    {
		printindent (f, level);
		FilePuts (f, "else\n");
		printStatement (f, e->tree.right->tree.right, level+1, level, nest);
	    }
	}
	break;
    case CATCH:
	printindent (f, level);
	FilePuts (f, "try");
	if (nest)
	{
	    FilePuts (f, "\n");
	    printStatement (f, e->tree.left, level+1, level, nest);
	}
	else
	    FilePuts (f, " ");
	while ((e = e->tree.right))
	{
	    FilePuts (f, "catch ");
	    FilePuts (f, AtomName (e->tree.left->code.code->base.name));
	    FilePuts (f, " ");
	    PrintCode (f, e->tree.left->code.code,
		       0, class_undef,
		       publish_private, level, nest);
	    if (!nest)
		FilePuts (f, "\n");
	}
	break;
    case RAISE:
	printindent (f, level);
	FilePuts (f, "raise ");
	FilePuts (f, AtomName (e->tree.left->atom.atom));
	FilePuts (f, " (");
	if (e->tree.right)
	    printParameters (f, e->tree.right, nest);
	FilePuts (f, ");\n");
	break;
    }
}

void
PrintCode (Value f, CodePtr code, Atom name, Class class, Publish publish,
	   int level, Bool nest)
{
    ArgType	*args;
    
    if (name)
    {
	printpublish (f, publish);
	printclass (f, class);
    }
    if (!TypePoly (code->base.type))
    {
	fprintTypes (f, code->base.type);
	FilePuts (f, " ");
    }
    if (name)
    {
	FilePuts (f, "function ");
	FilePuts (f, AtomName (name));
	FileOutput (f, ' ');
    }
    else
	FilePuts (f, "func ");
    FilePuts (f, "(");
    for (args = code->base.args; args; args = args->next)
    {
	if (!TypePoly (args->type) || !args->name)
	{
	    fprintTypes (f, args->type);
	    if (args->name)
		FilePuts (f, " ");
	}
	if (args->name)
	    FilePuts (f, AtomName (args->name));
	if (args->next || code->base.argc == -1)
	    FilePuts (f, ", ");
    }
    if (code->base.builtin && code->base.argc == -1)
	FilePuts (f, "...");
    FilePuts (f, ")");
    if (code->base.builtin)
    {
	FilePuts (f, " <builtin>");
    }
    else
    {
	if (nest)
	{
	    FilePuts (f, "\n");
	    printindent (f, level);
	    FilePuts (f, "{\n");
	    printBlock (f, code->func.code, level + 1, nest);
	    printindent (f, level);
	    FilePuts (f, "}");
	}
    }
}

void
PrintStat (Value f, Expr *e, Bool nest)
{
    printStatement (f, e, 1, 1, nest);
}

void
doPrettyPrint (Value f, Symbol *name, int level, Bool nest);
    
static void
PrintNamespace (Value f, NamespacePtr namespace, int level)
{
    NamespaceChainPtr   chain;

    for (chain = namespace->symbols; chain; chain = chain->next)
    {
	if (chain->publish == publish_public)
	    doPrettyPrint (f, chain->symbol, level, False);
    }
}

void
doPrettyPrint (Value f, Symbol *name, int level, Bool nest)
{
    printindent (f, level);
    switch (name->symbol.class) {
    case class_global:
	if (BoxValue (name->global.value, 0)->value.tag == type_func)
	{
	    PrintCode (f, BoxValue (name->global.value, 0)->func.code,
		       name->symbol.name,
		       class_undef,
		       name->symbol.publish,
		       level, nest);
	}
	else
	{
	    FilePuts (f, "(");
	    fprintTypes (f, name->symbol.type);
	    FilePuts (f, ") ");
	    print (f, BoxValue (name->global.value, 0));
	}
	FilePuts (f, "\n");
	break;
    case class_namespace:
	printpublish (f, name->symbol.publish);
	printclass (f, name->symbol.class);
	FilePuts (f, " ");
	FilePuts (f, AtomName (name->symbol.name));
	FilePuts (f, " {\n");
	PrintNamespace (f, name->namespace.namespace, level + 1);
	printindent (f, level);
	FilePuts (f, "}\n");
	break;
    case class_exception:
	printpublish (f, name->symbol.publish);
	printclass (f, name->symbol.class);
	FilePuts (f, " ");
	FilePuts (f, AtomName (name->symbol.name));
	FilePuts (f, " {\n");
	PrintNamespace (f, name->namespace.namespace, level + 1);
	printindent (f, level);
	FilePuts (f, "}\n");
	break;
    default:
	printpublish (f, name->symbol.publish);
	printclass (f, name->symbol.class);
	FilePuts (f, " ");
	if (!TypePoly (name->symbol.type))
	{
	    fprintTypes (f, name->symbol.type);
	    FilePuts (f, " ");
	}
	FilePuts (f, AtomName (name->symbol.name));
	FilePuts (f, ";\n");
	break;
    }
}

void
PrettyPrint (Value f, Symbol *name)
{
    doPrettyPrint (f, name, 0, True);
}
