/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

typedef struct _atom {
    DataType	    *data;
    struct _atom    *next;
} AtomEntry;

#define AtomEntryName(ae)   ((char *) ((ae) + 1))

static void
AtomEntryMark (void *object)
{
    ;
}

DataType    AtomEntryType = { AtomEntryMark, 0, "AtomEntryType" };

# define HASHSIZE	63

typedef struct _atomTable {
    DataType	    *data;
    AtomEntry	    *hash[HASHSIZE];
} AtomTable;

static void
AtomTableMark (void *object)
{
    AtomTable	*table = object;
    int		i;
    AtomEntry	*atom;

    for (i = 0; i < HASHSIZE; i++)
	for (atom = table->hash[i]; atom; atom = atom->next)
	    MemReference (atom);
}

DataType    AtomTableType = { AtomTableMark, 0, "AtomTableType" };

AtomTable   *atomTable;

int
AtomInit (void)
{
    ENTER ();
    atomTable = ALLOCATE (&AtomTableType, sizeof (AtomTable));
    memset (atomTable->hash, '\0', sizeof (atomTable->hash));
    MemAddRoot (atomTable);
    EXIT ();
    return 1;
}

static int
hash (char *name)
{
    int h;

    h = 0;
    while (*name)
	h += *name++;
    if (h < 0)
	h = -h;
    return h % HASHSIZE;
}

Atom
AtomId (char *name)
{
    AtomEntry	**bucket = &atomTable->hash[hash(name)];
    AtomEntry	*atomEntry;

    for (atomEntry = *bucket; atomEntry; atomEntry = atomEntry->next)
	if (!strcmp (name, AtomEntryName(atomEntry)))
	    break;
    if (!atomEntry)
    {
	ENTER ();
	atomEntry = ALLOCATE (&AtomEntryType, sizeof (AtomEntry) + strlen (name) + 1);
	atomEntry->next = *bucket;
	*bucket = atomEntry;
	strcpy (AtomEntryName(atomEntry), name);
	EXIT();
    }
    return AtomEntryName (atomEntry);
}

static void
AtomListMark (void *object)
{
    AtomListPtr	al = object;

    MemReference (al->next);
}

DataType AtomListType = { AtomListMark, 0, "AtomListType" };

AtomListPtr
NewAtomList (AtomListPtr next, Atom atom)
{
    ENTER ();
    AtomListPtr	al;

    al = ALLOCATE (&AtomListType, sizeof (AtomList));
    al->next = next;
    al->atom = atom;
    RETURN (al);
}
