/* $Header$ */

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * mem.c
 *
 * interface:
 *
 * void *MemAllocate (DataType *type, int size)
 *			- returns a hunk of at least "size" bytes
 * void MemReference (void *object)
 *			- marks the indicated object as referenced
 * DataType *MemType (void *object)
 *			- returns the type of the indicated object
 * void MemAddRoot (void *object)
 *			- adds the indicated object as a root
 * void MemCollect (void)
 *			- sweeps the entire heap freeing unused storage
 *
 * local routines:
 *
 * gimmeBlock()		- calls iGarbageCollect (if it is time) or malloc
 * allocBlock()		- calls malloc
 * disposeBlock()	- calls free
 *
 * tossFree()		- erases the free lists
 * checkRef()		- puts unreferenced hunks on free lists
 * checkBlockRef(b)	- puts unreferenced hunks in a block on free lists
 */

#include	<config.h>

#include	<memory.h>
#define MEM_NEED_ALLOCATE
#include	"mem.h"
#include	"memp.h"
#include	<stdlib.h>

#if HAVE_STDINT_H
#include	<stdint.h>
#define PtrToInt(p)	((int) (intptr_t) (p))
#else
#define PtrToInt(p)	((int) (p))
#endif

int		GarbageTime;

void		*TemporaryData;

#undef DEBUG

#ifdef DEBUG
int	GCdebug;
#endif

StackObject *MemStack;

struct bfree *freeList[NUMSIZES];

int	sinceGarbage;

static void **Roots;
static int  RootCount;

struct bfree		*lastFree[NUMSIZES];

static struct block	*head;

int	totalBytesFree;
int	totalBytesUsed;
int	totalObjectsFree;
int	totalObjectsUsed;
int	useMap[NUMSIZES+1];

static struct block *
MemNewBlock (unsigned size, int sizeIndex)
{
    struct block    *b;
    if (++sinceGarbage >= GarbageTime)
    {
	MemCollect ();
	if (sizeIndex >= 0 && freeList[sizeIndex])
	    return 0;
    }
    b = (struct block *) malloc (size);
    if (!b)
    {
	MemCollect ();
	if (sizeIndex >= 0 && freeList[sizeIndex])
	    return 0;
	b = (struct block *) malloc (size);
	if (!b)
	    panic (0, "out of memory - quiting\n");
    }
    return b;
}

void *
MemHunkMore (int sizeIndex)
{
    unsigned char	*new;
    struct block	*b;
    int			n;

    b = MemNewBlock ((unsigned) BLOCKSIZE, sizeIndex);
    if (!b)
	return freeList[sizeIndex];
    /*
     * fill in per-block data fields
     */
    b->sizeIndex = sizeIndex;
    b->next = head;
    head = b; 

    /*
     * put it's contents on the free list
     */
    new = HUNKS(b);
    n = NUMHUNK(sizeIndex) - 1;
    while (n--)
    {
	unsigned char	*next = new + HUNKSIZE(sizeIndex);
	((struct bfree *) new)->type = 0;
	((struct bfree *) new)->next = (struct bfree *) next;
	new = next;
    }
    ((struct bfree *) new)->type = 0;
    ((struct bfree *) new)->next = freeList[sizeIndex];
    return freeList[sizeIndex] = (struct bfree *) HUNKS(b);
}

/*
 * take care of giant requests
 */

void *
MemAllocateHuge (DataType *type, int size)
{
    struct block	*b;
    unsigned char	*data;

    b = MemNewBlock ((unsigned) (size + HEADSIZE), -1);
    
    b->sizeIndex = size;
    b->next = head;
    head = b;

    data = HUNKS(b);
    *((DataType **) data) = type;
    return data;
}

#ifdef MEM_TRACE
DataType	*allDataTypes;

void
MemAddType (DataType *type)
{
    DataType	**prev;

    for (prev = &allDataTypes; *prev; prev =&(*prev)->next)
	if (strcmp ((*prev)->name, type->name) >= 0)
	    break;
    type->next = *prev;
    type->added = 1;
    *prev = type;
}

static void
MemActiveReference (DataType *type, int size)
{
    type->active++;
    type->active_bytes += size;
}

static void
MemActiveReset (void)
{
    DataType	*type;

    for (type = allDataTypes; type; type = type->next)
    {
	type->active = 0;
	type->active_bytes = 0;
    }
}

void
MemActiveDump (void)
{
    DataType	*type;

    debug ("Active memory dump\n");
    debug ("%20.20s: %9s %12s %9s %12s\n",
	   "name", "total", "bytes", "active", "bytes");
    for (type = allDataTypes; type; type = type->next)
    {
	debug ("%20.20s: %9d %12lld %9d %12lld\n",
	       type->name, type->total, type->total_bytes,
	       type->active, type->active_bytes);
    }
}

#endif

/*
 * garbage collection scheme
 */

#define isReferenced(a)	    (*((PtrInt *)(a)) & 1)
#define clrReference(a)	    (*((PtrInt *)(a)) &= ~1)
#define setReference(a)	    (*((PtrInt *)(a)) |= 1)

/*
 * eliminate any remaining free list
 */

static void
tossFree (void)
{
    memset (freeList, '\0', NUMSIZES * sizeof(freeList[0]));
}

static int
busy (unsigned char *data)
{
    DataType	*type;

    if (isReferenced (data))
	return 1;
    type = TYPE(data);
    if (!type)
	return 0;
    if (!type->Free)
	return 0;
    if ((*type->Free) (data))
	return 0;
    return 1;
}

static int
checkBlockRef (struct block *b)
{
    int		    sizeIndex = b->sizeIndex;
    unsigned char   *data = HUNKS(b);
    
    if (sizeIndex >= NUMSIZES)
    {
	if (busy (data))
	{
	    clrReference(HUNKS(b));
	    totalObjectsUsed++;
	    totalBytesUsed += sizeIndex;
	    useMap[NUMSIZES]++;
#ifdef MEM_TRACE
	    MemActiveReference (TYPE(data), b->sizeIndex);
#endif
	    return 1;
	}
	else
	{
	    totalBytesFree += sizeIndex;
	    totalObjectsFree++;
	    return 0;
	}
    }
    else
    {
	struct bfree	*first = 0;
	struct bfree    **prev = &first;
	int		n = NUMHUNK(sizeIndex);
	int		anybusy = 0;
	int		size = HUNKSIZE(sizeIndex);

	while (n--)
	{
	    if (busy (data))
	    {
		clrReference(data);
#ifdef MEM_TRACE
		MemActiveReference (TYPE(data), size);
#endif
		totalObjectsUsed++;
		useMap[sizeIndex]++;
		totalBytesUsed += size;
		anybusy = 1;
	    }
	    else
	    {
		TYPE(data) = 0;
		*prev = (struct bfree *) data;
		prev = &((struct bfree *) data)->next;
		totalBytesFree += size;
		totalObjectsFree++;
	    }
	    data += size;
	}
	if (anybusy)
	{
	    *prev = freeList[sizeIndex];
	    freeList[sizeIndex] = first;
	    return 1;
	}
	else
	{
	    return 0;
	}
    }
}

/*
 * checkRef: rebuild the free lists from unused data
 */

static void
checkRef (void)
{
    int	i;
    struct block    *b, **p;

    for (i = 0; i < NUMSIZES; i++)
	lastFree[i] = 0;
    for (p = &head; (b = *p); )
    {
	if (checkBlockRef (b))
	    p = &b->next;
	else
	{
	    *p = b->next;
	    free (b);
	}
    }
    GarbageTime = GARBAGETIME/* - (totalBytesFree / BLOCKSIZE) */;
    if (GarbageTime < 10)
	GarbageTime = 10;
#ifdef DEBUG
    if (GCdebug) {
	debug ("GC: used: bytes %7d objects %7d\n",
	       totalBytesUsed, totalObjectsUsed);
	debug ("GC: free: bytes %7d objects %7d\n",
	       totalBytesFree, totalObjectsFree);
	for (i = 0; i <= NUMSIZES; i++)
	    debug ("used %5d: %7d\n",
		   i == NUMSIZES ? 0 : HUNKSIZE(i), useMap[i]);
	debug ("GC: GarbageTime set to %d\n", GarbageTime);
    }
#endif
}

void
MemInitialize (void)
{
#ifdef DEBUG
    if (getenv ("NICKLE_MEM_DEBUG"))
	GCdebug=1;
#endif
    if (!MemStack)
    {
	MemStack = StackCreate ();
	MemAddRoot (MemStack);
    }
    GarbageTime = GARBAGETIME;
}

void
MemReference (void *object)
{
    DataType	*type;

    if (!object)
	return;
    if (PtrToInt(object) & 3)
	return;
    if (!isReferenced (object))
    {
	type = TYPE(object);
	setReference(object);
	if (type && type->Mark)
	    (*type->Mark) (object);
    }
}

/*
 * Mark an object but don't recurse, returning
 * whether the object was previously referenced or not
 */

int
MemReferenceNoRecurse (void *object)
{
    if (!object)
	return 1;
    if (PtrToInt (object) & 3)
	return 1;
    if (isReferenced (object))
	return 1;
    setReference (object);
    return 0;
}

DataType *
MemType (void *object)
{
    return TYPE (object);
}

void
MemAddRoot (void *object)
{
    void    **roots;

    roots = malloc (sizeof (void *) * (RootCount + 1));
    if (!roots)
	panic ("out of memory");
    if (RootCount)
	memcpy (roots, Roots, RootCount * sizeof (void *));
    if (Roots)
	free (Roots);
    Roots = roots;
    Roots[RootCount++] = object;
}


void
MemCollect (void)
{
    void    	**roots;
    int		rootCount;

#ifdef DEBUG
    if (GCdebug)
	debug ("GC:\n");
#endif
    memset (useMap, '\0', sizeof useMap);
    totalBytesFree = 0;
    totalObjectsFree = 0;
    totalBytesUsed = 0;
    totalObjectsUsed = 0;
    sinceGarbage = 0;
#ifdef MEM_TRACE
    MemActiveReset ();
#endif
    tossFree ();
#ifdef DEBUG
    if (GCdebug)
	debug ("GC: reference objects\n");
#endif
    rootCount = RootCount;
    roots = Roots;
    while (rootCount--)
	MemReference (*roots++);
    if (TemporaryData)
	MemReference (TemporaryData);
    checkRef ();
}
