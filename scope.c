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
    MemReference (namespace->names);
}

DataType namespaceType = { NamespaceMark, 0 };

NamespacePtr
NewNamespace (NamespacePtr previous)
{
    ENTER ();
    NamespacePtr    namespace;

    namespace = ALLOCATE (&namespaceType, sizeof (Namespace));
    namespace->previous = previous;
    namespace->names = 0;
    namespace->publish = publish_public;
    RETURN (namespace);
}

static void
NameMark (void *object)
{
    NamePtr   chain = object;

    MemReference (chain->next);
    MemReference (chain->symbol);
}

DataType nameType = { NameMark, 0 };

NamePtr
NewName (NamePtr next, Atom atom)
{
    ENTER ();
    NamePtr   name;

    name = ALLOCATE (&nameType, sizeof (Name));
    name->next = next;
    name->atom = atom;
    name->symbol = 0;
    RETURN (name);
}

NamespacePtr	GlobalNamespace, CurrentNamespace;
ReferencePtr	CurrentNamespaceReference;
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
    CurrentCommands = 0;
    CurrentCommandsReference = NewReference ((void **) &CurrentCommands);
    MemAddRoot (CurrentCommandsReference);
    EXIT ();
}

NamePtr
NamespaceFindName (NamespacePtr namespace, Atom atom, Bool search)
{
    NamePtr name;

    do
    {
	for (name = namespace->names; name; name = name->next)
	    if (name->atom == atom &&
		(namespace->publish == publish_public ||
		 (name->symbol && name->publish == publish_public)))
		return name;
	namespace = namespace->previous;
    } while (search && namespace);
    return 0;
}

NamePtr
NamespaceNewName (NamespacePtr namespace, Atom atom)
{
    NamePtr name;
    NamePtr *prev;

    for (prev = &namespace->names; (name = *prev); prev = &name->next)
	if (name->atom == atom)
	{
	    *prev = name->next;
	    break;
	}

    name = NewName (namespace->names, atom);
    namespace->names = name;
    return name;
}

Bool
NamespaceRemoveName (NamespacePtr namespace, NamePtr name)
{
    NamePtr *prev;

    for (prev = &namespace->names; *prev; prev = &(*prev)->next)
	if (*prev == name)
	{
	    *prev = name->next;
	    return True;
	    break;
	}
    return False;
}

void
NamespaceImport (NamespacePtr namespace, NamespacePtr import, Publish publish)
{
    NamePtr	old, new;

    for (old = import->names; old; old = old->next)
    {
	if (old->symbol && old->publish == publish_public)
	{
	    new = NamespaceNewName (namespace, old->atom);
	    new->symbol = old->symbol;
	    new->publish = publish;
	}
    }
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
NamespaceLocate (Value names, NamespacePtr *namespacep, NamePtr *namep)
{
    int		    i;
    NamespacePtr    s;
    FramePtr	    f;
    Value	    string;
    NamePtr	    name = 0;
    SymbolPtr	    sym;
    Bool	    search = True;
    
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
	string = BoxValue (names->array.values, i);
	if (aborting)
	    return False;
	if (string->value.tag != type_string)
	{
	    RaiseStandardException (exception_invalid_argument,
				    "not string",
				    2,
				    NewInt (0), string);
	    return False;
	}
	name = NamespaceFindName (s, AtomId (StringChars (&string->string)),
				  search);
	search = False;
	if (!name || ! (sym = name->symbol))
	{
	    FilePrintf (FileStdout, "No symbol \"%s\" in scope\n",
			StringChars (&string->string));
	    return False;
	}
	if (i != names->array.dim[0] - 1)
	{
	    if (sym->symbol.class != class_namespace)
	    {
		FilePrintf (FileStdout, "\"%s\" is not a namespace\n",
			    StringChars (&string->string));
		return False;
	    }
	    s = sym->namespace.namespace;
	}
    }
    *namespacep = s;
    *namep = name;
    return True;
}

