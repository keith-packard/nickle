/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

static void
DebugAddVar (NamespacePtr namespace, char *string, Value v, Types *type)
{
    ENTER ();
    NamePtr	name;

    name = NamespaceFindName (namespace, AtomId (string), False);
    if (!name)
	name = NamespaceNewName (namespace, AtomId (string));
    
    name->symbol = NewSymbolGlobal (name->atom, type);
    name->publish = publish_private;
    BoxValueSet (name->symbol->global.value, 0, v);
    EXIT ();
}

static void
DebugAddCommand (char *function, Bool names)
{
    SymbolPtr	sym;
    NamePtr	name;

    name = NamespaceFindName (CurrentNamespace, AtomId (function), True);
    if (!name)
	return;
    sym = NameSymbol (name);
    if (sym && sym->symbol.class == class_global)
    {
	CurrentCommands = NewCommand (CurrentCommands, name->atom, 
				      BoxValue (sym->global.value, 0), 
				      names);
    }
}
		 
static void
DebugDeleteCommand (char *function)
{
    CurrentCommands = CommandRemove (CurrentCommands, AtomId (function));
}

static struct {
    char    *function;
    Bool    names;
} debugCommands[] = {
    {  "trace",	False, },
    { "up",	False, },
    { "down",	False, },
    { "done",	False, },
};

#define NUM_DEBUG_COMMANDS  (sizeof (debugCommands) / sizeof (debugCommands[0]))

static void
DebugAddCommands (void)
{
    int	i;
    for (i = 0; i < NUM_DEBUG_COMMANDS; i++)
	DebugAddCommand (debugCommands[i].function, debugCommands[i].names);
}

static void
DebugDeleteCommands (void)
{
    int	i;
    for (i = 0; i < NUM_DEBUG_COMMANDS; i++)
	DebugDeleteCommand (debugCommands[i].function);
}

Bool
DebugSetFrame (Value continuation, int offset)
{
    ENTER ();
    FramePtr	    frame;
    NamespacePtr    namespace;
    int		    n = offset;
    Bool	    ret;
    ExprPtr	    stat;
    
    frame = continuation->continuation.frame;
    stat = continuation->continuation.pc->base.stat;
    while (frame && frame->function->func.code->base.builtin)
    {
	stat = frame->savePc->base.stat;
	frame = frame->previous;
    }
    while (frame && n--)
    {
	stat = frame->savePc->base.stat;
	frame = frame->previous;
    }
    if (stat)
	namespace = stat->base.namespace;
    else
	namespace = GlobalNamespace;
    ret = False;
    if (frame && namespace)
    {
	ret = True;
	CurrentNamespace = NewNamespace (namespace);
	CurrentFrame = frame;
	NamespaceImport (CurrentNamespace, DebugNamespace, publish_public);
	DebugAddVar (CurrentNamespace, "cont", continuation, typesPrim[type_continuation]);
	DebugAddVar (CurrentNamespace, "frame", NewInt (offset), typesPrim[type_integer]);
    }
    EXIT ();
    return ret;
}

void
DebugStart (Value continuation)
{
    if (LexResetInteractive ())
    {
	if (DebugSetFrame (continuation, 0))
	    DebugAddCommands ();
    }
}

Value
do_Debug_done (void)
{
    ENTER ();
    CurrentNamespace = GlobalNamespace;
    CurrentFrame = 0;
    DebugDeleteCommands ();
    RETURN (Zero);
}

Value
do_Debug_up (void)
{
    ENTER ();
    Value   frame;
    Value   continuation;
    
    continuation = lookupVar ("cont");
    frame = lookupVar ("frame");
    if (continuation->continuation.value.tag == type_continuation && frame->value.tag == type_int)
    {
	if (DebugSetFrame (continuation, frame->ints.value + 1))
	    RETURN (One);
	FilePrintf (FileStderr, "Already at top\n");
    }
    RETURN (Zero);
}

Value
do_Debug_down (void)
{
    ENTER ();
    Value   frame;
    Value   continuation;
    
    continuation = lookupVar ("cont");
    frame = lookupVar ("frame");
    if (continuation->value.tag == type_continuation && frame->value.tag == type_int)
    {
	if (frame->ints.value <= 0)
	    FilePrintf (FileStderr, "Already at bottom\n");
	else if (DebugSetFrame (continuation, frame->ints.value - 1))
	    RETURN (One);
    }
    RETURN (Zero);
}

Value
do_Debug_dump (Value f)
{
    ENTER ();
    CodePtr code;
    
    code = f->func.code;
    if (code->base.builtin)
    {
	FilePuts (FileStdout, "<builtin>\n");
	RETURN (Zero);
    }
    if (code->func.staticInit.obj)
    {
	FilePuts (FileStdout, "Static initializers\n");
	ObjDump (code->func.staticInit.obj, 2);
	FilePuts (FileStdout, "\n");
    }
    FilePuts (FileStdout, "Function body\n");
    ObjDump (code->func.body.obj, 1);
    RETURN (Zero);
}
