/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

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
CommandPtr	CurrentCommands;
ReferencePtr	CurrentCommandsReference;

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
    CurrentCommands = 0;
    CurrentCommandsReference = NewReference ((void **) &CurrentCommands);
    MemAddRoot (CurrentCommandsReference);
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

static void
CommandMark (void *object)
{
    CommandPtr    command = object;

    MemReference (command->previous);
    MemReference (command->func);
}

DataType commandType = { CommandMark, 0 };

CommandPtr
NewCommand (CommandPtr previous, Atom name, Value func, Bool names)
{
    ENTER ();
    CommandPtr    command;

    command = ALLOCATE (&commandType, sizeof (*command));
    command->previous = previous;
    command->name = name;
    command->func = func;
    command->names = names;
    RETURN (command);
}

CommandPtr
CommandFind (CommandPtr command, Atom name)
{
    for(; command; command = command->previous)
	if (command->name == name)
	    return command;
    return 0;
}

CommandPtr
CommandRemove (CommandPtr command, Atom name)
{
    ENTER ();
    CommandPtr    *prev;

    for (prev = &command; *prev; prev = &(*prev)->previous)
	if ((*prev)->name == name)
	{
	    *prev = (*prev)->previous;
	    break;
	}
    RETURN (command);
}

Bool
NamespaceLocate (Value names, NamespacePtr   *namespacep, SymbolPtr *symbolp)
{
    int		    i;
    NamespacePtr    s;
    FramePtr	    f;
    Value	    name;
    SymbolPtr	    sym = 0;
    int		    depth;
    
    if (names->value.tag != type_array || names->array.ndim != 1)
    {
	RaiseStandardException (exception_invalid_argument,
				"not array of strings",
				2,
				NewInt (0), names);
	return False;
    }
    if (names->array.dim[0] == 0)
	return False;
    GetNamespace (&s, &f);
    for (i = 0; i < names->array.dim[0]; i++)
    {
	name = BoxValue (names->array.values, i);
	if (aborting)
	    return False;
	if (name->value.tag != type_string)
	{
	    RaiseStandardException (exception_invalid_argument,
				    "not string",
				    2,
				    NewInt (0), name);
	    return False;
	}
	sym = NamespaceFindSymbol (s, AtomId (StringChars (&name->string)),
				   &depth);
	if (!sym)
	{
	    FilePrintf (FileStdout, "No symbol \"%s\" in scope\n",
			StringChars (&name->string));
	    return False;
	}
	if (i != names->array.dim[0] - 1)
	{
	    if (sym->symbol.class != class_namespace)
	    {
		FilePrintf (FileStdout, "\"%s\" is not a namespace\n",
			    StringChars (&name->string));
		return False;
	    }
	    s = sym->namespace.namespace;
	}
    }
    *namespacep = s;
    *symbolp = sym;
    return True;
}

