/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"nickle.h"
#include	"ref.h"

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
