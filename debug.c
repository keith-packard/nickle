/*
 * $Id$
 *
 * Copyright 1996 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "nick.h"

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
