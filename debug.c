/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"nickle.h"

static void
DebugAddVar (NamespacePtr namespace, char *name, Value v)
{
    ENTER ();
    SymbolPtr  s;

    s = NamespaceAddSymbol (namespace, NewSymbolGlobal (AtomId(name), 0, 
							publish_private));
    BoxValueSet (s->global.value, 0, v);
    EXIT ();
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
    while (frame && frame->previous && n--)
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
	CurrentNamespace->debugger = True;
	CurrentFrame = frame;
	NamespaceImport (CurrentNamespace, DebugNamespace, publish_public);
	DebugAddVar (CurrentNamespace, "cont", continuation);
	DebugAddVar (CurrentNamespace, "frame", NewInt (offset));
    }
    EXIT ();
    return ret;
}

Value
do_Debug_done (void)
{
    ENTER ();
    CurrentNamespace = GlobalNamespace;
    CurrentFrame = 0;
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
    if (continuation->value.tag == type_continuation && frame->value.tag == type_int &&
	frame->ints.value > 0)
    {
	if (DebugSetFrame (continuation, frame->ints.value - 1))
	    RETURN (One);
    }
    RETURN (Zero);
}

Value
do_Debug_dump (Value f)
{
    ENTER ();
    CodePtr code;
    
    if (f->value.tag != type_func)
    {
	RaiseError ("dump: parameter should be function %v", f);
	RETURN (Zero);
    }
    code = f->func.code;
    if (code->base.builtin)
    {
	FilePuts (FileStdout, "<builtin>\n");
	RETURN (Zero);
    }
    if (code->func.staticInit)
    {
	FilePuts (FileStdout, "Static initializers\n");
	ObjDump (code->func.staticInit, 2);
	FilePuts (FileStdout, "\n");
    }
    FilePuts (FileStdout, "Function body\n");
    ObjDump (code->func.obj, 1);
    RETURN (Zero);
}
