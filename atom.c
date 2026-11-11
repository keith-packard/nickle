/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include "nickle.h"

typedef struct _atom {
    DataType *data;
} AtomEntry;

#define AtomEntryName(ae) ((char *)((ae) + 1))

static DataType AtomEntryType = { 0, 0, "AtomEntryType" };

/*
 * Atoms live in an open-addressed hash table, probed linearly. Atoms
 * are never removed, so there are no deleted slots to skip, and the
 * table doubles when it becomes half full.
 */
#define ATOM_TABLE_MIN 512

typedef struct _atomTable {
    DataType   *data;
    ptrdiff_t   size;  /* number of slots, a power of two */
    ptrdiff_t   count; /* number of atoms */
    AtomEntry **slots;
} AtomTable;

static void
AtomTableMark(void *object)
{
    AtomTable *table = object;
    ptrdiff_t  i;

    for (i = 0; i < table->size; i++)
        if (table->slots[i])
            MemReference(table->slots[i]);
}

static DataType   AtomTableType = { AtomTableMark, 0, "AtomTableType" };

static AtomTable *atomTable;

static AtomEntry **
AtomSlots(ptrdiff_t size)
{
    AtomEntry **slots = calloc((size_t)size, sizeof(AtomEntry *));

    if (!slots)
        MemExhausted();
    return slots;
}

int
AtomInit(void)
{
    ENTER();
    atomTable = ALLOCATE(&AtomTableType, sizeof(AtomTable));
    atomTable->size = ATOM_TABLE_MIN;
    atomTable->count = 0;
    atomTable->slots = AtomSlots(ATOM_TABLE_MIN);
    MemAddRoot(atomTable);
    EXIT();
    return 1;
}

/* FNV-1a */
static HashValue
hash(const char *name)
{
    uint32_t h = 2166136261u;

    while (*name) {
        h ^= (unsigned char)*name++;
        h *= 16777619u;
    }
    return h;
}

/*
 * Return the slot holding 'name' in 'slots', or the empty slot where
 * it belongs
 */
static AtomEntry **
AtomSlot(AtomEntry **slots, ptrdiff_t size, const char *name)
{
    ptrdiff_t mask = size - 1;
    ptrdiff_t i = (ptrdiff_t)(hash(name) & (HashValue)mask);

    while (slots[i] && strcmp(name, AtomEntryName(slots[i])))
        i = (i + 1) & mask;
    return &slots[i];
}

static void
AtomTableGrow(void)
{
    ptrdiff_t   size = atomTable->size * 2;
    AtomEntry **slots = AtomSlots(size);
    ptrdiff_t   i;

    for (i = 0; i < atomTable->size; i++) {
        AtomEntry *atom = atomTable->slots[i];

        if (atom)
            *AtomSlot(slots, size, AtomEntryName(atom)) = atom;
    }
    free(atomTable->slots);
    atomTable->slots = slots;
    atomTable->size = size;
}

Atom
AtomId(char *name)
{
    AtomEntry **slot = AtomSlot(atomTable->slots, atomTable->size, name);
    AtomEntry  *atomEntry = *slot;

    if (!atomEntry) {
        ENTER();
        atomEntry = ALLOCATE(&AtomEntryType, sizeof(AtomEntry) + strlen(name) + 1);
        strcpy(AtomEntryName(atomEntry), name);
        *slot = atomEntry;
        if (++atomTable->count * 2 > atomTable->size)
            AtomTableGrow();
        EXIT();
    }
    return AtomEntryName(atomEntry);
}

static void
AtomListMark(void *object)
{
    AtomListPtr al = object;

    MemReference(al->next);
}

static DataType AtomListType = { AtomListMark, 0, "AtomListType" };

AtomListPtr
NewAtomList(AtomListPtr next, Atom atom)
{
    ENTER();
    AtomListPtr al;

    al = ALLOCATE(&AtomListType, sizeof(AtomList));
    al->next = next;
    al->atom = atom;
    RETURN(al);
}
