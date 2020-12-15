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
 * void MemAddRoot (void *object)
 *			- adds the indicated object as a root
 * void MemCollect (void)
 *			- sweeps the entire heap freeing unused storage
 *
 */

#include	<memory.h>
#define MEM_NEED_ALLOCATE 1
#include	"nickle-config.h"
#include	"mem.h"
#include	"memp.h"
#include	<stdlib.h>

static struct block *head;
struct bfree	    *freeList[NUMSIZES];

static int	    garbageTime;
static int	    sinceGarbage;

StackObject	    *MemStack;
void		    *TemporaryData;
static void	    **Roots;
static int	    RootCount;
static int	    RootSize;

#ifdef DEBUG
int		    GCdebug;
int		    totalBytesFree;
int		    totalBytesUsed;
int		    totalObjectsFree;
int		    totalObjectsUsed;
int		    useMap[NUMSIZES+1];
#endif

/*
 * Allocate a new block
 */

static struct block *
newBlock (int sizeIndex)
{
    struct block    *b;
    int		    size;
    
    if (sizeIndex < NUMSIZES)
	size = BLOCKSIZE;
    else
	size = sizeIndex + HEADSIZE;

    if (++sinceGarbage >= garbageTime)
    {
	MemCollect ();
	if (sizeIndex < NUMSIZES && freeList[sizeIndex])
	    return 0;
    }
    b = (struct block *) malloc (size);
    if (!b)
    {
	MemCollect ();
	if (sizeIndex < NUMSIZES && freeList[sizeIndex])
	    return 0;
	b = (struct block *) malloc (size);
	if (!b)
	    panic ("Out of memory\n");
    }
    b->sizeIndex = sizeIndex;
    b->next = head;
    head = b; 
    return b;
}

/*
 * Allocate a small block of memory
 */

struct bfree *
MemAllocateHunk (int sizeIndex)
{
    struct block	*b;
    struct bfree    	*new;

    b = newBlock (sizeIndex);
    if (b)
    {
	unsigned char	*data = HUNKS (b);
	int		n = NUMHUNK(sizeIndex);
	/*
	 * put the contents on the free list
	 */
	while (--n)
	{
	    unsigned char	*next = data + HUNKSIZE(sizeIndex);
	    
	    ((struct bfree *) data)->type = 0;
	    ((struct bfree *) data)->next = (struct bfree *) next;
	    data = next;
	}
	((struct bfree *) data)->type = 0;
	((struct bfree *) data)->next = freeList[sizeIndex];
	freeList[sizeIndex] = (struct bfree *) HUNKS(b);
    }
    new = freeList[sizeIndex];
    freeList[sizeIndex] = new->next;
    return new;
}

void *
MemAllocate (DataType *type, int size)
{
    int	sizeIndex = 0;
    struct bfree    *new;
    
#if NUMSIZES > 15    
    bad NUMSIZES
#endif
#ifdef MEM_TRACE
    if (!type->added)
	MemAddType (type);
    type->total++;
    type->total_bytes += size;
    type->active++;
    type->active_bytes += size;
#endif
    if (size > HUNKSIZE(7))
	sizeIndex += 8;
    if (size > HUNKSIZE(sizeIndex+3))
	sizeIndex += 4;
    if (size > HUNKSIZE(sizeIndex+1))
	sizeIndex += 2;
    if (size > HUNKSIZE(sizeIndex))
	sizeIndex += 1;
    
    if (sizeIndex >= NUMSIZES)
	new = MemAllocateHuge (size);
    else if ((new = freeList[sizeIndex]))
	freeList[sizeIndex] = new->next;
    else
	new = MemAllocateHunk (sizeIndex);
    new->type = type;
    return new;
}

void *
MemAllocateRef (DataType *type, int size)
{
    struct bfree    *new = MemAllocate (type, size);

    new->type = 0;
    REFERENCE (new);
    new->type = type;
    return (void *) new;
}

/*
 * Allocate a large block of memory
 */

struct bfree *
MemAllocateHuge (int size)
{
    return (struct bfree *) HUNKS (newBlock (size));
}

#ifdef MEM_TRACE
/*
 * Allocation tracing.  Track usage of each type of object
 */
static DataType	*allDataTypes;

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
activeReference (DataType *type, int size)
{
    type->active++;
    type->active_bytes += size;
}

static void
activeReset (void)
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
 * Initialize the allocator
 */

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
    garbageTime = GARBAGETIME;
}

/*
 * Add a root to the memory system, objects
 * referenced through this will be marked as busy
 */

void
MemAddRoot (void *object)
{
    void    **roots;

    if (RootCount == RootSize)
    {
	if (RootSize == 0)
	    RootSize = 128;
	else
	    RootSize *= 2;
	roots = malloc (sizeof (void *) * RootSize);
	if (!roots)
	    panic ("Out of memory\n");
	memcpy (roots, Roots, RootCount * sizeof (void *));
	if (Roots)
	    free (Roots);
	Roots = roots;
    }
    Roots[RootCount++] = object;
}

/*
 * Mark an object as referenced, recurse through
 * the Mark routine for the type to mark referenced objects
 */

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

/*
 * mark: walk roots marking referenced memory
 */

static void
mark (void)
{
    int	    rootCount = RootCount;
    void    **roots = Roots;
    
    while (rootCount--)
	MemReference (*roots++);
    if (TemporaryData)
	MemReference (TemporaryData);
}

static inline int
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

/*
 * sweep: rebuild the free lists from unused data
 */

static void
sweep (void)
{
    struct block    *b, **p;

    /* Erase free list */
    memset (freeList, '\0', NUMSIZES * sizeof(freeList[0]));
    /*
     * Walk all blocks
     */
    for (p = &head; (b = *p); )
    {
	int		sizeIndex = b->sizeIndex;
	int		n = NUMHUNK_ALL(sizeIndex);
	int		size = HUNKSIZE_ALL(sizeIndex);
	unsigned char   *data = HUNKS(b);
	struct bfree    *first = 0;
	struct bfree    **prev = &first;
	int		anybusy = 0;

	while (n--)
	{
	    if (busy (data))
	    {
		clrReference(data);
		anybusy = 1;
#ifdef MEM_TRACE
		activeReference (TYPE(data), size);
#endif
#ifdef DEBUG
		totalObjectsUsed++;
		useMap[sizeIndex]++;
		totalBytesUsed += size;
#endif
	    }
	    else
	    {
		TYPE(data) = 0;
		*prev = (struct bfree *) data;
		prev = &((struct bfree *) data)->next;
#ifdef DEBUG
		totalBytesFree += size;
		totalObjectsFree++;
#endif
	    }
	    data += size;
	}
	if (anybusy)
	{
	    if (sizeIndex < NUMSIZES)
	    {
		*prev = freeList[sizeIndex];
		freeList[sizeIndex] = first;
	    }
	    p = &b->next;
	}
	else
	{
	    *p = b->next;
	    free (b);
	}
    }
}

/*
 * Garbage collect
 */

void
MemCollect (void)
{
#ifdef DEBUG
    if (GCdebug)
	debug ("GC:\n");
    memset (useMap, '\0', sizeof useMap);
    totalBytesFree = 0;
    totalObjectsFree = 0;
    totalBytesUsed = 0;
    totalObjectsUsed = 0;
#endif
    sinceGarbage = 0;
    
#ifdef MEM_TRACE
    activeReset ();
#endif
    
    /*
     * Mark
     */
#ifdef DEBUG
    if (GCdebug)
	debug ("GC: mark objects\n");
#endif
    mark ();
    
    /*
     * Sweep
     */
#ifdef DEBUG
    if (GCdebug)
	debug ("GC: sweep objects\n");
#endif
    sweep ();
    
    /*
     * Set the garbage collection time
     */
    garbageTime = GARBAGETIME/* - (totalBytesFree / BLOCKSIZE) */;
    if (garbageTime < 10)
	garbageTime = 10;

#ifdef DEBUG
    if (GCdebug) {
	int i;
	debug ("GC: used: bytes %7d objects %7d\n",
	       totalBytesUsed, totalObjectsUsed);
	debug ("GC: free: bytes %7d objects %7d\n",
	       totalBytesFree, totalObjectsFree);
	for (i = 0; i <= NUMSIZES; i++)
	    debug ("used %5d: %7d\n",
		   i == NUMSIZES ? 0 : HUNKSIZE(i), useMap[i]);
	debug ("GC: garbageTime set to %d\n", garbageTime);
    }
#endif
}

