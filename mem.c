/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
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
 * noteBlock(b)		- permits address->block translation for these objects
 * unNoteBlock(b)	- removes block from data structure
 * addrToBlock(a,bp,ip)	- translates address (a) to block (*bp) and index (*ip)
 * blockWalk(f)		- calls f with each active block
 *
 * clearRef()		- clears all ref bits in the system
 * clearBlockRef(b)	- clears all ref bits in a particular block
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

static void	noteBlock (struct block *);
static void	checkBlockRef (struct block *);
#ifdef DEBUG
static void	verifyBlock (void);
#endif

int		GarbageTime;

void		*TemporaryData;

#ifdef DEBUG
int	GCdebug;
#endif

StackObject *MemStack;

struct bfree *freeList[NUMSIZES];

int	sinceGarbage;

static void **Roots;
static int  RootCount;

struct bfree	*lastFree[NUMSIZES];

struct block	*hughFree;

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
    unsigned char	*limit;
    struct block	*b;
    int			bsize;
    int			dsize;

    b = MemNewBlock ((unsigned) BLOCKSIZE, sizeIndex);
    if (!b)
	return freeList[sizeIndex];
    /*
     * fill in per-block data fields
     */
    bsize = HUNKSIZE(sizeIndex);
    b->sizeIndex = sizeIndex;
    b->bitmap = ((unsigned char *) b) + HEADSIZE;
    b->bitmapsize = BITMAPSIZE(bsize);
    b->data = (unsigned char *)
	((((PtrInt) (b->bitmap + b->bitmapsize)) + (MINHUNK-1))
	 & (~(MINHUNK-1)));
    dsize = (((unsigned char *) b) + BLOCKSIZE) - b->data;
    b->datasize = (dsize / bsize) * bsize;
    /*
     * put this block into the address converter
     */
    noteBlock (b);
    /*
     * put it's contents on the free list
     */
    limit = b->data + (b->datasize - bsize);
    for (new = b->data; new < limit; new += bsize) {
	((struct bfree *) new)->type = 0;
	((struct bfree *) new)->next = (struct bfree *) (new + bsize);
    }
    ((struct bfree *) new)->type = 0;
    ((struct bfree *) new)->next = freeList[sizeIndex];
    return freeList[sizeIndex] = (struct bfree *) b->data;
}

/*
 * take care of giant requests
 */

void *
MemAllocateHuge (DataType *type, int size)
{
    struct block	*huge;

    huge = MemNewBlock ((unsigned) (size + HEADSIZE), -1);
    huge->sizeIndex = NUMSIZES;
    huge->bitmapsize = 0;
    huge->datasize = size;
    huge->bitmap = 0;
    huge->data = (void *) (huge + 1);
    noteBlock (huge);
    *((DataType **) huge->data) = type;
    return huge->data;
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
 * address->(block,index) translation scheme.  three routines:
 *
 * noteBlock	- install a new block into the database
 * addrToBlock	- translate an address into (block, index)
 * blockWalk	- call a routine with every referenced block
 */

static struct block	*root;

static void
noteBlock (struct block *b)
{
    (void) tree_insert (&root, b);
#ifdef DEBUG
    if (GCdebug)
	verifyBlock();
#endif
}

static void
unNoteBlock (struct block *b)
{
    (void) tree_delete (&root, b);
#ifdef DEBUG
    if (GCdebug)
	verifyBlock();
#endif
}

#ifdef DEBUG
static void
verifyBlock (void)
{
    if (!tree_verify (root))
	abort ();
}
#endif
			    
static void
blockTreeWalk (struct block *treep,
	       void	    (*function)(struct block *))
{
    if (treep) {
	blockTreeWalk (treep->left, function);
	function (treep);
	blockTreeWalk (treep->right, function);
    }
}

static void
blockWalk (void (*function) (struct block *))
{
    blockTreeWalk (root, function);
}

/*
 * garbage collection scheme
 */

/*
 * note that setObjectRef has had addrToBlock inlined for speed
 */

static int
setReference (void *address)
{
    register struct block	*b;
    register int		dist;
    register int		byte;
    register int		bit;
    register int    		index;
    register unsigned char	*map;
    int				old;

    /*
     * a very simple cache -- this results
     * in about 16% speed increase in this
     * routine best case.  Worse case it
     * results in about a 5% decrease from
     * wasted computation
     */
    static struct block	*cache;

    b = cache;
    if ((b = cache) &&
	(PtrInt) address >= (PtrInt) b->data &&
	(PtrInt) address <= (PtrInt) b->data + b->datasize)
	goto cache_hit;

    for (b = root; b;) {
	if ((PtrInt) address < (PtrInt) b->data)
	    b = b->left;
	else if ((PtrInt) address >= (PtrInt) b->data + b->datasize)
	    b = b->right;
	else {
	    cache = b;
	cache_hit:		;
	    if ((map = b->bitmap)) {
		dist = (PtrInt) address - (PtrInt) b->data;
		index = dist / HUNKSIZE(b->sizeIndex);
		byte = index >> 3;
		bit = (1 << (index & 7));
		old = map[byte] & bit;
		map[byte] |= bit;
	    } else
		old = b->ref;
	    b->ref = 1;
#ifdef MEM_TRACE
	    if (!old)
		MemActiveReference (TYPE(address),
				    map ? HUNKSIZE(b->sizeIndex) :
				    b->datasize);
#endif
	    return old;
	}
    }
    return 1;
}

/*
 * Verify that address through address+size are in
 * same object as base
 */
void
MemCheckPointer (void *base, void *address, int size)
{
    struct block    *b;
    int		    datasize;

    for (b = root; b;) {
	if ((PtrInt) address < (PtrInt) b->data)
	    b = b->left;
	else if ((PtrInt) address >= (PtrInt) b->data + b->datasize)
	    b = b->right;
	else
	{
	    if (b->bitmap)
		datasize = HUNKSIZE(b->sizeIndex);
	    else
		datasize = b->datasize;
	    if ((PtrInt) base <= (PtrInt) address && 
		(PtrInt) address + size <= (PtrInt) base + datasize)
	    {
		return;
	    }
	    abort ();
	}
    }
    abort ();
}

void
MemFree (void *address, int size)
{
    int	sizeIndex = 0;

#if NUMSIZES > 16
    bad NUMSIZES
#endif
    if (size)
    {
	if (size > HUNKSIZE(7))
	    sizeIndex += 8;
	if (size > HUNKSIZE(sizeIndex+3))
	    sizeIndex += 4;
	if (size > HUNKSIZE(sizeIndex+1))
	    sizeIndex += 2;
	if (size > HUNKSIZE(sizeIndex))
	    sizeIndex += 1;
    }
    else
    {
	struct block    *b;

	for (b = root; b;) {
	    if ((PtrInt) address < (PtrInt) b->data)
		b = b->left;
	    else if ((PtrInt) address >= (PtrInt) b->data + b->datasize)
		b = b->right;
	    else
	    {
		if (b->bitmap)
		    sizeIndex = b->sizeIndex;
		else
		    sizeIndex = NUMSIZES;
		break;
	    }
	}
    }

    if (sizeIndex >= NUMSIZES)
    {
	struct block    *b = (struct block *) address - 1;
	unNoteBlock (b);
	free (b);
    }
    else
    {
	struct bfree    *old = address;

	old->next = freeList[sizeIndex];
	freeList[sizeIndex] = old;
    }
}

/*
 * clearRef: zero's the reference bit for all objects
 */

static void
clearBlockRef (struct block *b)
{
    int	i;

    if (b->bitmap)
	for (i = 0; i < b->bitmapsize; i++)
	    b->bitmap[i] = 0;
    b->ref = 0;
}

static void
clearRef (void)
{
    blockWalk (clearBlockRef);
}

/*
 * eliminate any remaining free list
 */

static void
tossFree (void)
{
    int	i;

    for (i = 0; i < NUMSIZES; i++)
	freeList[i] = 0;
}

/*
 * checkRef: rebuild the free lists from unused data
 */

static void
checkBlockRef (struct block *b)
{
    int		    sizeIndex;
    int		    size;
    unsigned char   *byte;
    int		    bit;
    unsigned char   *object, *max;
    struct bfree    *thisLast;
    DataType	    *type;

    if (!b->ref) {
#ifdef DEBUG
	if (GCdebug > 1)
	    debug ("unreferenced block at 0x%x\n", b);
#endif
	if (b->sizeIndex == NUMSIZES)
	{
	    object = b->data;
	    type = TYPE(object);
	    if (type && type->Free)
		(*type->Free) (object);
	    TYPE(object) = 0;
	    totalBytesFree += b->datasize;
	    totalObjectsFree++;
	}
	else
	{
	    totalBytesFree += b->datasize;
	    totalObjectsFree += b->datasize / HUNKSIZE(b->sizeIndex);
	}
	b->bitmap = (unsigned char *) hughFree;
	hughFree = b;
	
    } else if (b->bitmap) {
	sizeIndex = b->sizeIndex;
	thisLast = lastFree[sizeIndex];
	max = b->data + b->datasize;
	size = HUNKSIZE(sizeIndex);
	byte = b->bitmap;
	bit = 1;
	for (object = b->data; object < max; object += size) {
	    if (!(*byte & bit)) {
		type = TYPE(object);
		if (type && type->Free)
		    (*type->Free) (object);
		TYPE(object) = 0;
		if (thisLast)
		    thisLast->next = (struct bfree *) object;
		else
		    freeList[sizeIndex] = (struct bfree *) object;
		thisLast = (struct bfree *) object;
		totalBytesFree += size;
		totalObjectsFree++;
	    }
	    else 
	    {
		totalObjectsUsed++;
		useMap[sizeIndex]++;
		totalBytesUsed += size;
	    }
	    bit <<= 1;
	    if (bit == (1 << BITSPERCH)) {
		bit = 1;
		byte++;
	    }
	}
	if (thisLast)
	    thisLast->next = 0;
	lastFree[sizeIndex] = thisLast;
    } else {
	useMap[NUMSIZES]++;
	totalBytesUsed += b->datasize;
    }
}

static void
checkRef (void)
{
    int	i;
    struct block	*n;

    for (i = 0; i < NUMSIZES; i++)
	lastFree[i] = 0;
    hughFree = 0;
    blockWalk (checkBlockRef);
    while (hughFree) {
	n = (struct block *) hughFree->bitmap;
	unNoteBlock (hughFree);
	free (hughFree);
	hughFree = n;
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
	    debug ("used %4d: %7d\n",
		   i == NUMSIZES ? 0 : HUNKSIZE(i), useMap[i]);
	debug ("GC: GarbageTime set to %d\n", GarbageTime);
    }
#endif
}

void
MemInitialize (void)
{
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
    if (((int) object) & 3)
	return;
    if (!setReference (object))
    {
	type = TYPE(object);
	if (type && type->Mark)
	    (*type->Mark) (object);
    }
}

int
MemReferenceNoRecurse (void *object)
{
    if (!object)
	return 1;
    if (((int) object) & 3)
	return 1;
    return setReference (object);
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
    clearRef ();
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
#ifdef DEBUG
    if (GCdebug) {
	debug ("GC: verifyBlock\n");
	verifyBlock ();
    }
#endif
}

