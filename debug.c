/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

static void
DebugAddVar (NamespacePtr namespace, char *string, Value v, Type *type)
{
    ENTER ();
    SymbolPtr	symbol;

    symbol = NamespaceAddName (namespace,
			       NewSymbolGlobal (AtomId (string), type),
			       publish_private);
    BoxValueSet (symbol->global.value, 0, v);
    EXIT ();
}

static void
DebugAddCommand (char *function, Bool names)
{
    SymbolPtr	symbol;

    symbol = NamespaceFindName (CurrentNamespace, AtomId (function), True);
    if (symbol && symbol->symbol.class == class_global)
    {
	CurrentCommands = NewCommand (CurrentCommands, symbol->symbol.name, 
				      BoxValue (symbol->global.value, 0), 
				      names);
    }
}
		 
static void
DebugDeleteCommand (char *function)
{
    CurrentCommands = CommandRemove (CurrentCommands, AtomId (function));
}

static const struct {
    char    *function;
    Bool    names;
} debugCommands[] = {
    {  "trace",	False, },
    { "up",	False, },
    { "down",	False, },
    { "done",	False, },
    { "help",	False, },
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
    FramePtr	    frame = continuation->continuation.frame;
    ObjPtr	    obj = continuation->continuation.obj;
    InstPtr	    pc = continuation->continuation.pc;
    ExprPtr	    stat = obj ? ObjStatement (obj, pc) : 0;
    NamespacePtr    namespace;
    int		    n = offset;
    Bool	    ret;
    
    while (frame && frame->function->func.code->base.builtin)
    {
	stat = ObjStatement (frame->saveObj,
			     frame->savePc);
	frame = frame->previous;
    }
    while (frame && n--)
    {
	stat = ObjStatement (frame->saveObj,
			     frame->savePc);
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
	TopNamespace = CurrentNamespace = NewNamespace (namespace);
	CurrentFrame = frame;
	NamespaceImport (CurrentNamespace, DebugNamespace, publish_public);
	DebugAddVar (CurrentNamespace, "cont", continuation, typePrim[rep_continuation]);
	DebugAddVar (CurrentNamespace, "frame", NewInt (offset), typePrim[rep_integer]);
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
    TopNamespace = CurrentNamespace = GlobalNamespace;
    CurrentFrame = 0;
    DebugDeleteCommands ();
    RETURN (Void);
}

Value
do_Debug_help (void)
{
    ENTER ();
    FilePrintf (FileStderr,
		"debug commands: trace, up, down, done, help\n");
    RETURN (Void);
}

Value
do_Debug_up (void)
{
    ENTER ();
    Value   frame;
    Value   continuation;
    
    continuation = lookupVar (0, "cont");
    frame = lookupVar (0, "frame");
    if (ValueIsContinuation(continuation) && ValueIsInt(frame))
    {
	if (DebugSetFrame (continuation, ValueInt(frame) + 1))
	    RETURN (TrueVal);
	FilePrintf (FileStderr, "Already at top\n");
    }
    RETURN (FalseVal);
}

Value
do_Debug_down (void)
{
    ENTER ();
    Value   frame;
    Value   continuation;
    
    continuation = lookupVar (0, "cont");
    frame = lookupVar (0, "frame");
    if (ValueIsContinuation(continuation) && ValueIsInt(frame))
    {
	if (ValueInt(frame) <= 0)
	    FilePrintf (FileStderr, "Already at bottom\n");
	else if (DebugSetFrame (continuation, ValueInt(frame) - 1))
	    RETURN (TrueVal);
    }
    RETURN (FalseVal);
}

Value
do_Debug_dump (Value f)
{
    ENTER ();
    CodePtr code;
    
    if (!ValueIsFunc (f))
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("dump: not a function"),
				NewInt (0), f);
	RETURN (Void);
    }
    code = f->func.code;
    if (code->base.builtin)
    {
	FilePuts (FileStdout, "<builtin>\n");
	RETURN (Void);
    }
    if (code->func.staticInit.obj)
    {
	FilePuts (FileStdout, "Static initializers\n");
	ObjDump (code->func.staticInit.obj, 2);
	FilePuts (FileStdout, "\n");
    }
    FilePuts (FileStdout, "Function body\n");
    ObjDump (code->func.body.obj, 1);
    RETURN (Void);
}

#ifdef MEM_TRACE
Value
do_Debug_dump_active (void)
{
    MemCollect ();
    MemActiveDump ();
    return Void;
}
#endif
