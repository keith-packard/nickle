/*
 * $Id$
 *
 * Copyright 1996 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "value.h"

static int  nextAtomValue;

typedef struct _atom {
    DataType	    *data;
    struct _atom    *next;
    int		    id;
} AtomEntry;

#define AtomEntryName(ae)   ((char *) ((ae) + 1))

void
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

void
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

int
hash (char *name)
{
    register h;

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
