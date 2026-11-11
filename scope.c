/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include "nickle.h"
#include "ref.h"

static void
NamespaceMark(void *object)
{
    NamespacePtr namespace = object;

    MemReference(namespace->previous);
    MemReference(namespace->names);
    MemReference(namespace->hash);
}

static DataType namespaceType = { NamespaceMark, 0, "namespaceType" };

NamespacePtr
NewNamespace(NamespacePtr previous)
{
    ENTER();
    NamespacePtr namespace;

    namespace = ALLOCATE(&namespaceType, sizeof(Namespace));
    namespace->previous = previous;
    namespace->names = 0;
    namespace->publish = publish_public;
    namespace->count = 0;
    namespace->hash = 0;
    RETURN(namespace);
}

static void
NamelistMark(void *object)
{
    NamelistPtr namelist = object;

    MemReference(namelist->next);
    MemReference(namelist->symbol);
}

static DataType namelistType = { NamelistMark, 0, "namelistType" };

static NamelistPtr
NewNamelist(NamelistPtr next, SymbolPtr symbol, Publish publish)
{
    ENTER();
    NamelistPtr namelist;

    namelist = ALLOCATE(&namelistType, sizeof(Namelist));
    namelist->next = next;
    namelist->hashNext = 0;
    namelist->symbol = symbol;
    namelist->publish = publish;
    RETURN(namelist);
}

/*
 * Namespaces with this many names get an index, so finding and adding
 * names doesn't search the whole list
 */
#define NAMESPACE_HASH_MIN 16

/* The chains reference names which are all in the list, so mark nothing */
static DataType namelistHashType = { 0, 0, "namelistHashType" };

static ptrdiff_t
NamelistChain(NamelistHash *hash, Atom atom)
{
    /* Atoms are distinct pointers; mix their address bits */
    HashValue h = (HashValue)(uintptr_t)atom;

    h ^= h >> 7;
    h *= 2654435761u;
    h ^= h >> 13;
    return (ptrdiff_t)(h & (HashValue)(hash->size - 1));
}

static void
NamelistHashInsert(NamelistHash *hash, NamelistPtr namelist)
{
    NamelistPtr *chain = &hash->chains[NamelistChain(hash, namelist->symbol->symbol.name)];

    namelist->hashNext = *chain;
    *chain = namelist;
}

static void
NamelistHashRemove(NamelistHash *hash, NamelistPtr namelist)
{
    NamelistPtr *prev;

    for (prev = &hash->chains[NamelistChain(hash, namelist->symbol->symbol.name)]; *prev;
         prev = &(*prev)->hashNext)
        if (*prev == namelist) {
            *prev = namelist->hashNext;
            break;
        }
}

/*
 * Index the names of 'namespace' in a table of 'size' chains
 */
static void
NamespaceRehash(NamespacePtr namespace, ptrdiff_t size)
{
    ENTER();
    NamelistHash *hash;
    NamelistPtr   namelist;

    hash = ALLOCATE(&namelistHashType, sizeof(NamelistHash) + (size_t)size * sizeof(NamelistPtr));
    hash->size = size;
    memset(hash->chains, '\0', (size_t)size * sizeof(NamelistPtr));
    for (namelist = namespace->names; namelist; namelist = namelist->next)
        NamelistHashInsert(hash, namelist);
    namespace->hash = hash;
    EXIT();
}

/*
 * Find the name 'atom' in 'namespace' alone
 */
static NamelistPtr
NamespaceLookup(NamespacePtr namespace, Atom atom)
{
    NamelistPtr namelist;

    if (namespace->hash)
        namelist = namespace->hash->chains[NamelistChain(namespace->hash, atom)];
    else
        namelist = namespace->names;
    while (namelist) {
        if (namelist->symbol->symbol.name == atom)
            return namelist;
        namelist = namespace->hash ? namelist->hashNext : namelist->next;
    }
    return 0;
}

/*
 * Remove 'old' from the names of 'namespace'
 */
static void
NamespaceUnlink(NamespacePtr namespace, NamelistPtr old)
{
    NamelistPtr *prev;

    for (prev = &namespace->names; *prev; prev = &(*prev)->next)
        if (*prev == old) {
            *prev = old->next;
            break;
        }
    if (namespace->hash)
        NamelistHashRemove(namespace->hash, old);
    namespace->count--;
}

NamespacePtr        GlobalNamespace, TopNamespace, CurrentNamespace;
static ReferencePtr TopNamespaceReference;
static ReferencePtr CurrentNamespaceReference;
CommandPtr          CurrentCommands;
static ReferencePtr CurrentCommandsReference;
static ReferencePtr CurrentFrameReference;

void
NamespaceInit(void)
{
    ENTER();

    GlobalNamespace = NewNamespace(0);
    MemAddRoot(GlobalNamespace);

    TopNamespace = GlobalNamespace;
    TopNamespaceReference = NewReference((void **)&TopNamespace);
    MemAddRoot(TopNamespaceReference);

    CurrentNamespace = GlobalNamespace;
    CurrentNamespaceReference = NewReference((void **)&CurrentNamespace);
    MemAddRoot(CurrentNamespaceReference);

    CurrentCommands = 0;
    CurrentCommandsReference = NewReference((void **)&CurrentCommands);
    MemAddRoot(CurrentCommandsReference);

    CurrentFrameReference = NewReference((void **)&CurrentFrame);
    MemAddRoot(CurrentFrameReference);

    EXIT();
}

static NamelistPtr
NamespaceFindNamelist(NamespacePtr namespace, Atom atom, Bool search, Bool allow_private)
{
    NamelistPtr namelist;

    /* Each name appears at most once in a namespace */
    do {
        namelist = NamespaceLookup(namespace, atom);
        if (namelist
            && (allow_private || namespace->publish != publish_private
                || namelist->publish != publish_private))
            return namelist;
        namespace = namespace->previous;
    } while (search && namespace);
    return 0;
}

SymbolPtr
NamespaceFindName(NamespacePtr namespace, Atom atom, Bool search)
{
    NamelistPtr namelist;

    namelist = NamespaceFindNamelist(namespace, atom, search, False);
    if (namelist)
        return namelist->symbol;
    return 0;
}

Bool
NamespaceIsNamePrivate(NamespacePtr namespace, Atom atom, Bool search)
{
    return NamespaceFindNamelist(namespace, atom, search, True) != 0;
}

SymbolPtr
NamespaceAddName(NamespacePtr namespace, SymbolPtr symbol, Publish publish)
{
    ENTER();
    NamelistPtr namelist;

    /*
     * Remove old symbol
     */
    namelist = NamespaceLookup(namespace, symbol->symbol.name);
    if (namelist)
        NamespaceUnlink(namespace, namelist);

    namelist = NewNamelist(namespace->names, symbol, publish);
    namespace->names = namelist;
    namespace->count++;
    if (namespace->hash) {
        if (namespace->count > namespace->hash->size)
            NamespaceRehash(namespace, namespace->hash->size * 2);
        else
            NamelistHashInsert(namespace->hash, namelist);
    } else if (namespace->count >= NAMESPACE_HASH_MIN)
        NamespaceRehash(namespace, NAMESPACE_HASH_MIN * 2);
    EXIT();
    return symbol;
}

Bool
NamespaceRemoveName(NamespacePtr namespace, Atom atom)
{
    NamelistPtr namelist = NamespaceLookup(namespace, atom);

    if (!namelist)
        return False;
    NamespaceUnlink(namespace, namelist);
    return True;
}

void
NamespaceImport(NamespacePtr namespace, NamespacePtr import, Publish publish)
{
    NamelistPtr namelist;

    for (namelist = import->names; namelist; namelist = namelist->next)
        if (namelist->publish == publish_public)
            NamespaceAddName(namespace, namelist->symbol, publish);
}

static void
CommandMark(void *object)
{
    CommandPtr command = object;

    MemReference(command->previous);
    MemReference(command->func);
}

static DataType commandType = { CommandMark, 0, "commandType" };

CommandPtr
NewCommand(CommandPtr previous, Atom name, Value func, Bool names)
{
    ENTER();
    CommandPtr command;

    command = ALLOCATE(&commandType, sizeof(*command));
    command->previous = previous;
    command->name = name;
    command->func = func;
    command->names = names;
    RETURN(command);
}

CommandPtr
CommandFind(CommandPtr command, Atom name)
{
    for (; command; command = command->previous)
        if (command->name == name)
            return command;
    return 0;
}

CommandPtr
CommandRemove(CommandPtr command, Atom name)
{
    ENTER();
    CommandPtr *prev;

    for (prev = &command; *prev; prev = &(*prev)->previous)
        if ((*prev)->name == name) {
            *prev = (*prev)->previous;
            break;
        }
    RETURN(command);
}

Bool
NamespaceLocate(Value names, NamespacePtr *namespacep, SymbolPtr *symbolp, Publish *publishp,
                Bool complain)
{
    ptrdiff_t    i;
    NamespacePtr namespace;
    FramePtr     f;
    CodePtr      c;
    Value        string;
    NamelistPtr  namelist = 0;
    SymbolPtr    symbol;
    Bool         search = True;

    if (!ValueIsArray(names) || names->array.ndim != 1 || ArrayLimits(&names->array)[0] == 0) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("not non-empty array of strings"), NewInt(0), names);
        return False;
    }
    GetNamespace(&namespace, &f, &c);
    for (i = 0; i < ArrayLimits(&names->array)[0]; i++) {
        string = ArrayValue(&names->array, i);
        if (aborting)
            return False;
        if (!ValueIsString(string)) {
            RaiseStandardException(exception_invalid_argument, 3, NewStrString("not string"),
                                   NewInt(0), string);
            return False;
        }
        namelist
            = NamespaceFindNamelist(namespace, AtomId(StringChars(&string->string)), search, True);
        search = False;
        if (!namelist) {
            if (complain)
                FilePrintf(FileStdout, "No symbol %v in namespace\n", string);
            return False;
        }
        symbol = namelist->symbol;
        if (i != ArrayLimits(&names->array)[0] - 1) {
            if (symbol->symbol.class != class_namespace) {
                RaiseStandardException(exception_invalid_argument, 3, NewStrString("not namespace"),
                                       NewPtrdiffInteger(i), string);
                return False;
            }
            namespace = symbol->namespace.namespace;
        }
    }
    /* names is non-empty, so the loop has found a namelist */
    if (!namelist)
        return False;
    *namespacep = namespace;
    *symbolp = namelist->symbol;
    *publishp = namelist->publish;
    ;
    return True;
}
