/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

static int  nextAtomValue;

typedef struct _atom {
    DataType	    *data;
    struct _atom    *next;
    int		    id;
} AtomEntry;

#define AtomEntryName(ae)   ((char *) ((ae) + 1))

static void
AtomEntryMark (void *object)
{
    ;
}

DataType    AtomEntryType = { AtomEntryMark, 0 };

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

DataType    AtomTableType = { AtomTableMark, 0 };

AtomTable   *atomTable;

int
AtomInit (void)
{
    ENTER ();
    atomTable = ALLOCATE (&AtomTableType, sizeof (AtomTable));
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
	atomEntry->id = ++nextAtomValue;
	strcpy (AtomEntryName(atomEntry), name);
	EXIT();
    }
    return atomEntry->id;
}

char *
AtomName (Atom id)
{
    int		i;
    AtomEntry	*atomEntry;

    for (i = 0; i < HASHSIZE; i++)
	for (atomEntry = atomTable->hash[i]; atomEntry; atomEntry = atomEntry->next)
	    if (atomEntry->id == id)
		return AtomEntryName (atomEntry);
    return 0;
}

static void
AtomListMark (void *object)
{
    AtomListPtr	al = object;

    MemReference (al->next);
}

DataType AtomListType = { AtomListMark, 0 };

AtomListPtr
NewAtomList (AtomListPtr next, Atom id)
{
    ENTER ();
    AtomListPtr	al;

    al = ALLOCATE (&AtomListType, sizeof (AtomList));
    al->next = next;
    al->id = id;
    RETURN (al);
}
