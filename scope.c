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
NamespaceMark (void *object)
{
    NamespacePtr    namespace = object;

    MemReference (namespace->previous);
    MemReference (namespace->symbols);
    MemReference (namespace->code);
}

DataType namespaceType = { NamespaceMark, 0 };

NamespacePtr
NewNamespace (NamespacePtr previous)
{
    ENTER ();
    NamespacePtr    namespace;

    namespace = ALLOCATE (&namespaceType, sizeof (Namespace));
    namespace->previous = previous;
    namespace->symbols = 0;
    namespace->code = 0;
    namespace->publish = publish_public;
    namespace->debugger = False;
    RETURN (namespace);
}

static void
NamespaceChainMark (void *object)
{
    NamespaceChainPtr   chain = object;

    MemReference (chain->next);
    MemReference (chain->symbol);
}

DataType namespaceChainType = { NamespaceChainMark, 0 };

static NamespaceChainPtr
NewNamespaceChain (NamespaceChainPtr next, SymbolPtr symbol, Publish publish)
{
    ENTER ();
    NamespaceChainPtr   chain;

    chain = ALLOCATE (&namespaceChainType, sizeof (NamespaceChain));
    chain->next = next;
    chain->symbol = symbol;
    chain->publish = publish;
    RETURN (chain);
}

NamespacePtr	GlobalNamespace, CurrentNamespace;
ReferencePtr	CurrentNamespaceReference;

void
NamespaceInit (void)
{
    ENTER ();

    GlobalNamespace = NewNamespace (0);
    MemAddRoot (GlobalNamespace);
    CurrentNamespace = GlobalNamespace;
    CurrentNamespaceReference = NewReference ((void **) &CurrentNamespace);
    MemAddRoot (CurrentNamespaceReference);
    EXIT ();
}

SymbolPtr
NamespaceLookupSymbol (NamespacePtr namespace, Atom name)
{
    NamespaceChainPtr   chain;
    
    for (chain = namespace->symbols; chain; chain = chain->next)
	if (chain->symbol->symbol.name == name &&
	    (namespace->publish == publish_public ||
	     chain->publish == publish_public))
	    return chain->symbol;
    return 0;
}

SymbolPtr
NamespaceFindSymbol (NamespacePtr namespace, Atom name, int *depth)
{
    int		    d;
    SymbolPtr	    s;

    d = 0;
    while (namespace)
    {
	s = NamespaceLookupSymbol (namespace, name);
	if (s)
	{
	    *depth = d;
	    return s;
	}
	if (namespace->code)
	    d++;
	namespace = namespace->previous;
    }
    return 0;
}

SymbolPtr
NamespaceAddSymbol (NamespacePtr namespace, SymbolPtr symbol)
{
    ENTER ();
    NamespaceChainPtr   chain;

    chain = NewNamespaceChain (namespace->symbols, symbol, symbol->symbol.publish);
    namespace->symbols = chain;
    RETURN (symbol);
}

Bool
NamespaceRemoveSymbol (NamespacePtr namespace, SymbolPtr symbol)
{
    NamespaceChainPtr	*prev;

    for (prev = &namespace->symbols; *prev; prev = &(*prev)->next)
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
NamespaceImport (NamespacePtr namespace, NamespacePtr import, Publish publish)
{
    NamespaceChainPtr   chain;
    SymbolPtr		symbol;

    for (chain = import->symbols; chain; chain = chain->next)
    {
	if (chain->publish == publish_public)
	{
	    symbol = chain->symbol;
	    if (NamespaceLookupSymbol (namespace, symbol->symbol.name))
		return symbol;
	    namespace->symbols = NewNamespaceChain (namespace->symbols, symbol, publish);
	}
    }
    return 0;
}
