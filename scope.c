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
#include "ref.h"

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
NewScope (ScopePtr previous)
{
    ENTER ();
    ScopePtr	scope;

    scope = ALLOCATE (&scopeType, sizeof (Scope));
    scope->previous = previous;
    scope->symbols = 0;
    scope->code = 0;
    scope->publish = publish_public;
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
NewScopeChain (ScopeChainPtr next, SymbolPtr symbol, Publish publish)
{
    ENTER ();
    ScopeChainPtr   chain;

    chain = ALLOCATE (&scopeChainType, sizeof (ScopeChain));
    chain->next = next;
    chain->symbol = symbol;
    chain->publish = publish;
    RETURN (chain);
}

ScopePtr	GlobalScope, CurrentScope;
ReferencePtr	CurrentScopeReference;

void
ScopeInit (void)
{
    ENTER ();

    GlobalScope = NewScope (0);
    MemAddRoot (GlobalScope);
    CurrentScope = GlobalScope;
    CurrentScopeReference = NewReference ((void **) &CurrentScope);
    MemAddRoot (CurrentScopeReference);
    EXIT ();
}

SymbolPtr
ScopeLookupSymbol (ScopePtr scope, Atom name)
{
    ScopeChainPtr   chain;
    
    for (chain = scope->symbols; chain; chain = chain->next)
	if (chain->symbol->symbol.name == name &&
	    (scope->publish == publish_public ||
	     chain->publish == publish_public))
	    return chain->symbol;
    return 0;
}

SymbolPtr
ScopeFindSymbol (ScopePtr scope, Atom name, int *depth)
{
    int		    d;
    SymbolPtr	    s;

    d = 0;
    while (scope)
    {
	s = ScopeLookupSymbol (scope, name);
	if (s)
	{
	    *depth = d;
	    return s;
	}
	if (scope->code)
	    d++;
	scope = scope->previous;
    }
    return 0;
}

SymbolPtr
ScopeAddSymbol (ScopePtr scope, SymbolPtr symbol)
{
    ENTER ();
    ScopeChainPtr   chain;

    chain = NewScopeChain (scope->symbols, symbol, symbol->symbol.publish);
    scope->symbols = chain;
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
	    return True;
	}
    }
    return False;
}

SymbolPtr
ScopeImport (ScopePtr scope, ScopePtr import, Publish publish)
{
    ScopeChainPtr   chain;
    SymbolPtr	    symbol;

    for (chain = import->symbols; chain; chain = chain->next)
    {
	if (chain->publish == publish_public)
	{
	    symbol = chain->symbol;
	    if (ScopeLookupSymbol (scope, symbol->symbol.name))
		return symbol;
	    scope->symbols = NewScopeChain (scope->symbols, symbol, publish);
	}
    }
    return 0;
}
