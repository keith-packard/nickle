/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"nickle.h"

void
DebugAddVar (ScopePtr scope, char *name, Value v)
{
    ENTER ();
    SymbolPtr  s;

    s = ScopeAddSymbol (scope, NewSymbolGlobal (AtomId(name), type_undef, 
						publish_private));
    BoxValue (s->global.value, 0) = v;
    EXIT ();
}

Bool
DebugSetFrame (Value thread, int offset)
{
    ENTER ();
    FramePtr	frame;
    ScopePtr	scope;
    int		n = offset;
    Bool	ret;
    ExprPtr	stat;
    
    frame = thread->thread.frame;
    stat = thread->thread.pc->base.stat;
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
	scope = stat->base.scope;
    else
	scope = GlobalScope;
    ret = False;
    if (frame && scope)
    {
	ret = True;
	CurrentScope = NewScope (scope);
	CurrentFrame = frame;
	ScopeImport (CurrentScope, DebugScope, publish_public);
	DebugAddVar (CurrentScope, "thread", thread);
	DebugAddVar (CurrentScope, "frame", NewInt (offset));
    }
    EXIT ();
    return ret;
}

Value
DebugDone (void)
{
    ENTER ();
    CurrentScope = GlobalScope;
    CurrentFrame = 0;
    RETURN (Zero);
}

Value
DebugUp (void)
{
    ENTER ();
    Value   frame;
    Value   thread;
    
    thread = lookupVar ("thread");
    frame = lookupVar ("frame");
    if (thread->value.tag == type_thread && frame->value.tag == type_int)
    {
	if (DebugSetFrame (thread, frame->ints.value + 1))
	    RETURN (One);
    }
    RETURN (Zero);
}

Value
DebugDown (void)
{
    ENTER ();
    Value   frame;
    Value   thread;
    
    thread = lookupVar ("thread");
    frame = lookupVar ("frame");
    if (thread->value.tag == type_thread && frame->value.tag == type_int &&
	frame->ints.value > 0)
    {
	if (DebugSetFrame (thread, frame->ints.value - 1))
	    RETURN (One);
    }
    RETURN (Zero);
}
