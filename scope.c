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
TypespacePtr	CurrentTypespace;
ReferencePtr	CurrentTypespaceReference;

void
NamespaceInit (void)
{
    ENTER ();

    GlobalNamespace = NewNamespace (0);
    MemAddRoot (GlobalNamespace);
    CurrentNamespace = GlobalNamespace;
    CurrentNamespaceReference = NewReference ((void **) &CurrentNamespace);
    MemAddRoot (CurrentNamespaceReference);
    CurrentTypespace = 0;
    CurrentTypespaceReference = NewReference ((void **) &CurrentTypespace);
    MemAddRoot (CurrentTypespaceReference);
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

Class
NamespaceDefaultClass (NamespacePtr namespace)
{
    while (namespace && !namespace->code && !namespace->debugger)
	namespace = namespace->previous;
    if (namespace && !namespace->debugger)
	return class_auto;
    return class_global;
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
	    namespace->symbols = NewNamespaceChain (namespace->symbols, 
						    symbol, publish);
	}
    }
    return 0;
}

static void
TypespaceMark (void *object)
{
    TypespacePtr    typespace = object;

    MemReference (typespace->previous);
}

DataType typespaceType = { TypespaceMark, 0 };

TypespacePtr
NewTypespace (TypespacePtr previous, Atom name, Bool mask)
{
    ENTER ();
    TypespacePtr    typespace;

    typespace = ALLOCATE (&typespaceType, sizeof (Typespace));
    typespace->previous = previous;
    typespace->name = name;
    typespace->mask = mask;
    RETURN (typespace);
}

TypespacePtr
TypespaceFind (TypespacePtr typespace, Atom name)
{
    for(; typespace; typespace = typespace->previous)
	if (typespace->name == name)
	{
	    if (typespace->mask)
		break;
	    return typespace;
	}
    return 0;
}

TypespacePtr
TypespaceRemove (TypespacePtr typespace, Atom name)
{
    ENTER ();
    TypespacePtr    *prev;

    for (prev = &typespace; *prev; prev = &(*prev)->previous)
	if ((*prev)->name == name)
	{
	    *prev = (*prev)->previous;
	    break;
	}
    RETURN (typespace);
}
