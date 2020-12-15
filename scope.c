/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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

DataType namespaceType = { NamespaceMark, 0, "namespaceType" };

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
NamelistMark (void *object)
{
    NamelistPtr	namelist = object;

    MemReference (namelist->next);
    MemReference (namelist->symbol);
}

DataType namelistType = { NamelistMark, 0, "namelistType" };

static NamelistPtr
NewNamelist (NamelistPtr next, SymbolPtr symbol, Publish publish)
{
    ENTER ();
    NamelistPtr   namelist;

    namelist = ALLOCATE (&namelistType, sizeof (Namelist));
    namelist->next = next;
    namelist->symbol = symbol;
    namelist->publish = publish;
    RETURN (namelist);
}

NamespacePtr	GlobalNamespace, TopNamespace, CurrentNamespace;
ReferencePtr	TopNamespaceReference;
ReferencePtr	CurrentNamespaceReference;
CommandPtr	CurrentCommands;
ReferencePtr	CurrentCommandsReference;

void
NamespaceInit (void)
{
    ENTER ();

    GlobalNamespace = NewNamespace (0);
    MemAddRoot (GlobalNamespace);
    
    TopNamespace = GlobalNamespace;
    TopNamespaceReference = NewReference ((void **) &TopNamespace);
    MemAddRoot (TopNamespaceReference);
    
    CurrentNamespace = GlobalNamespace;
    CurrentNamespaceReference = NewReference ((void **) &CurrentNamespace);
    MemAddRoot (CurrentNamespaceReference);
    
    CurrentCommands = 0;
    CurrentCommandsReference = NewReference ((void **) &CurrentCommands);
    MemAddRoot (CurrentCommandsReference);

    EXIT ();
}

static NamelistPtr
NamespaceFindNamelist (NamespacePtr namespace, Atom atom, Bool search, Bool allow_private)
{
    NamelistPtr	namelist;

    do
    {
	for (namelist = namespace->names; namelist; namelist = namelist->next)
	    if (namelist->symbol->symbol.name == atom &&
		(allow_private ||
		 namespace->publish != publish_private ||
		 namelist->publish != publish_private))
		return namelist;
	namespace = namespace->previous;
    } while (search && namespace);
    return 0;
}

SymbolPtr
NamespaceFindName (NamespacePtr namespace, Atom atom, Bool search)
{
    NamelistPtr	namelist;

    namelist = NamespaceFindNamelist (namespace, atom, search, False);
    if (namelist)
	return namelist->symbol;
    return 0;
}

Bool
NamespaceIsNamePrivate (NamespacePtr namespace, Atom atom, Bool search)
{
    return NamespaceFindNamelist (namespace, atom, search, True) != 0;
}

SymbolPtr
NamespaceAddName (NamespacePtr namespace, SymbolPtr symbol, Publish publish)
{
    NamelistPtr namelist;
    NamelistPtr	*prev;

    /*
     * Remove old symbol
     */
    for (prev = &namespace->names; (namelist = *prev); prev = &namelist->next)
	if (namelist->symbol->symbol.name == symbol->symbol.name)
	{
	    *prev = namelist->next;
	    break;
	}

    namelist = NewNamelist (namespace->names, symbol, publish);
    namespace->names = namelist;
    return symbol;
}

Bool
NamespaceRemoveName (NamespacePtr namespace, Atom atom)
{
    NamelistPtr	namelist, *prev;

    for (prev = &namespace->names; (namelist = *prev); prev = &namelist->next)
	if (namelist->symbol->symbol.name == atom)
	{
	    *prev = namelist->next;
	    return True;
	    break;
	}
    return False;
}

void
NamespaceImport (NamespacePtr namespace, NamespacePtr import, Publish publish)
{
    NamelistPtr	namelist;

    for (namelist = import->names; namelist; namelist = namelist->next)
	if (namelist->publish == publish_public)
	    NamespaceAddName (namespace, namelist->symbol, publish);
}

static void
CommandMark (void *object)
{
    CommandPtr    command = object;

    MemReference (command->previous);
    MemReference (command->func);
}

DataType commandType = { CommandMark, 0, "commandType" };

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
NamespaceLocate (Value		names, 
		 NamespacePtr	*namespacep,
		 SymbolPtr	*symbolp,
		 Publish	*publishp,
		 Bool		complain)
{
    int		    i;
    NamespacePtr    namespace;
    FramePtr	    f;
    CodePtr	    c;
    Value	    string;
    NamelistPtr	    namelist = 0;
    SymbolPtr	    symbol;
    Bool	    search = True;
    
    if (!ValueIsArray(names) || names->array.ndim != 1 || 
	ArrayLimits(&names->array)[0] == 0)
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("not non-empty array of strings"),
				NewInt (0), names);
	return False;
    }
    GetNamespace (&namespace, &f, &c);
    for (i = 0; i < ArrayLimits(&names->array)[0]; i++)
    {
	string = ArrayValue (&names->array, i);
	if (aborting)
	    return False;
	if (!ValueIsString(string))
	{
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("not string"),
				    NewInt (0), string);
	    return False;
	}
	namelist = NamespaceFindNamelist (namespace, 
					  AtomId (StringChars (&string->string)),
					  search, True);
	search = False;
	if (!namelist)
	{
	    if (complain)
		FilePrintf (FileStdout, "No symbol %v in namespace\n",
			    string);
	    return False;
	}
	symbol = namelist->symbol;
	if (i != ArrayLimits(&names->array)[0] - 1)
	{
	    if (symbol->symbol.class != class_namespace)
	    {
		RaiseStandardException (exception_invalid_argument, 3,
					NewStrString ("not namespace"),
					NewInt(i), string);
		return False;
	    }
	    namespace = symbol->namespace.namespace;
	}
    }
    *namespacep = namespace;
    *symbolp = namelist->symbol;
    *publishp = namelist->publish;;
    return True;
}

