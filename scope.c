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

static void
ScopeMark (void *object)
{
    ScopePtr	scope = object;

    MemReference (scope->previous);
    MemReference (scope->symbols);
    MemReference (scope->code);
}

DataType scopeType = { ScopeMark, 0 };

ScopePtr
NewScope (ScopePtr previous, ScopeType type)
{
    ENTER ();
    ScopePtr	scope;

    scope = ALLOCATE (&scopeType, sizeof (Scope));
    scope->previous = previous;
    scope->symbols = 0;
    scope->count = 0;
    scope->type = type;
    scope->code = 0;
    RETURN (scope);
}

static void
ScopeChainMark (void *object)
{
    ScopeChainPtr   chain = object;

    MemReference (chain->next);
    MemReference (chain->symbol);
}

DataType scopeChainType = { ScopeChainMark, 0 };

ScopeChainPtr
NewScopeChain (ScopeChainPtr next, SymbolPtr symbol)
{
    ENTER ();
    ScopeChainPtr   chain;

    chain = ALLOCATE (&scopeChainType, sizeof (ScopeChain));
    chain->next = next;
    chain->symbol = symbol;
    RETURN (chain);
}

ScopePtr	GlobalScope;

void
ScopeInit (void)
{
    ENTER ();

    GlobalScope = NewScope (0, ScopeGlobal);
    MemAddRoot (GlobalScope);
    EXIT ();
}

SymbolPtr
ScopeFindSymbol (ScopePtr scope, Atom name, int *depth)
{
    ScopeChainPtr   chain;
    int		    d;

    d = 0;
    while (scope)
    {
	for (chain = scope->symbols; chain; chain = chain->next)
	    if (chain->symbol->symbol.name == name)
	    {
		*depth = d;
		return chain->symbol;
	    }
	scope = scope->previous;
	if (scope && scope->type == ScopeFrame)
	    d++;
    }
    return 0;
}

SymbolPtr
ScopeAddSymbol (ScopePtr scope, SymbolPtr symbol)
{
    ENTER ();
    ScopeChainPtr   chain;

    chain = NewScopeChain (scope->symbols, symbol);
    scope->symbols = chain;
    switch (symbol->symbol.class) {
    case class_static:
	if (scope->type != ScopeStatic)
	    break;
	symbol->local.element = scope->count;
	scope->count++;
	break;
    case class_arg:
    case class_auto:
	if (scope->type != ScopeFrame)
	    break;
	symbol->local.element = scope->count;
	scope->count++;
	break;
    default:
	break;
    }
    RETURN (symbol);
}

Bool
ScopeRemoveSymbol (ScopePtr scope, SymbolPtr symbol)
{
    ScopeChainPtr	*prev;

    for (prev = &scope->symbols; *prev; prev = &(*prev)->next)
    {
	if ((*prev)->symbol == symbol)
	{
	    *prev = (*prev)->next;
	    scope->count--;
	    return True;
	}
    }
    return False;
}
