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
 * sizeToIndex(size)	- returns the bucket number for a particular size
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

# include	<memory.h>
# include	"mem.h"
# include	"avl.h"
# include	"memp.h"


static void	*allocBlock (void);
static void	*gimmeBlock (void);
static void	*allocHuge(int size);
static void	*gimmeHuge (int size);
static void	noteBlock (struct block *);
static void	checkBlockRef (struct block *);
#ifdef DEBUG
static void	verifyBlock (void);
#endif
extern void	*malloc (unsigned);
extern void	free (void *);

int		GarbageTime = GARBAGETIME;

void		*TemporaryData;

#ifdef DEBUG
int	GCdebug;
#endif

static int sizeMap[NUMSIZES] = {
    MINHUNK,
    MINHUNK*2,
    MINHUNK*4,
    MINHUNK*8,
    MINHUNK*16,
    MINHUNK*32,
    MINHUNK*64,
    MINHUNK*128,
    MAXHUNK,
};

StackObject *MemStack;

static struct bfree *freeList[NUMSIZES];

int	sinceGarbage = 0;

static void **Roots;
static int  RootCount;

struct bfree	*lastFree[NUMSIZES];

struct block	*hughFree;

int	totalBytesFree;
int	totalBytesUsed;
int	totalObjectsFree;
int	totalObjectsUsed;
int	useMap[NUMSIZES+1];


static INLINE void *
newHunk (int sizeIndex)
{
    register char	*new;
    char		*limit;
    struct block	*b;
    int			bsize;
    int			dsize;

    if (!freeList[sizeIndex]) {
	bsize = sizeMap[sizeIndex];
	b = (struct block *) gimmeBlock ();
	if (!b) {
	    if (freeList[sizeIndex])
		goto gotsome;
	    b = (struct block *) allocBlock ();
	    if (!b)
		panic (0, "out of memory - quiting\n");
	}
	/*
	 * fill in per-block data fields
	 */
	b->sizeIndex = sizeIndex;
	b->bitmap = ((char *) b) + HEADSIZE;
	b->bitmapsize = BITMAPSIZE(bsize);
	b->data = (char *)
	    ((((PtrInt) (b->bitmap + b->bitmapsize)) + (MINHUNK-1))
	     & (~(MINHUNK-1)));
	dsize = (((char *) b) + BLOCKSIZE) - b->data;
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
	freeList[sizeIndex] = (struct bfree *) b->data;
    }
gotsome:
    new = (char *) freeList[sizeIndex];
    freeList[sizeIndex] = freeList[sizeIndex]->next;
#ifdef DEBUG
    memset (new, '\0', sizeMap[sizeIndex]);
#endif
    return new;
}

static INLINE int
sizeToIndex (int size)
{
    int	sizeIndex;

    for (sizeIndex = 0; sizeIndex < NUMSIZES; sizeIndex++)
	if (sizeMap[sizeIndex] >= size)
	    return sizeIndex;
    return -1;
}

/*
 * take care of giant requests
 */
static INLINE void *
newHuge (int size)
{
    struct block	*huge;

    huge = (struct block *) gimmeHuge (HEADSIZE + size);
    if (!huge) {
	huge = (struct block *) allocHuge
	(HEADSIZE + size);
	if (!huge)
	    panic (0, "out of memory - quiting\n");
    }
    huge->sizeIndex = NUMSIZES;
    huge->bitmapsize = 0;
    huge->datasize = size;
    huge->bitmap = 0;
    huge->data = (void *) (huge + 1);
    noteBlock (huge);
    if (((PtrInt) huge) & 01)
	abort ();
    return huge->data;
}

static INLINE void *
newObject (int size)
{
    int	sizeIndex;

    sizeIndex = sizeToIndex (size);
    if (sizeIndex == -1)
	return newHuge (size);
    return newHunk (sizeIndex);
}

static void *
gimmeBlock (void)
{
    void	*result;

    if (++sinceGarbage >= GarbageTime) {
	MemCollect ();
	sinceGarbage = 0;
	return 0;
    }
    result = allocBlock ();
    if (!result) {
	MemCollect ();
	sinceGarbage = 0;
    }
    return result;
}

static void *
allocBlock (void)
{
    return malloc ((unsigned) BLOCKSIZE);
}

static INLINE void
disposeBlock (struct block *b)
{
    free ((char *) b);
}

static void *
gimmeHuge (int size)
{
    char	*result;

    if (++sinceGarbage >= GarbageTime) {
	MemCollect ();
	sinceGarbage = 0;
	return 0;
    }
    result = allocHuge (size);
    if (!result) {
	MemCollect ();
	sinceGarbage = 0;
    }
    return result;
}

static void *
allocHuge (int size)
{
    return malloc ((unsigned) size);
}

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
    register			index;
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

    if ((b = cache) &&
	(dist = ((PtrInt) address) - ((PtrInt) b->data)) > 0 &&
	dist < b->datasize)
	goto cache_hit;

    for (b = root; b;) {
	if ((dist = ((PtrInt) address) - ((PtrInt) b->data)) < 0)
	    b = b->left;
	else if (dist >= b->datasize)
	    b = b->right;
	else {
	    cache = b;
	cache_hit:		;
	    if ((map = b->bitmap)) {
		index = dist / sizeMap[b->sizeIndex];
		byte = index >> 3;
		bit = (1 << (index & 7));
		old = map[byte] & bit;
		map[byte] |= bit;
	    } else
		old = b->ref;
	    b->ref = 1;
	    return old;
	}
    }
    return 1;
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
    int			    sizeIndex;
    int			    size;
    register char	    *byte;
    register int	    bit;
    register char	    *object, *max;
    register struct bfree   *thisLast;
    DataType		    *type;

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
	    totalObjectsFree += b->datasize / sizeMap[b->sizeIndex];
	}
	b->bitmap = (char *) hughFree;
	hughFree = b;
	
    } else if (b->bitmap) {
	sizeIndex = b->sizeIndex;
	thisLast = lastFree[sizeIndex];
	max = b->data + b->datasize;
	size = sizeMap[sizeIndex];
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
	disposeBlock (hughFree);
	hughFree = n;
    }
    GarbageTime = GARBAGETIME - (totalBytesFree / GOODBLOCKSIZE);
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
		   i == NUMSIZES ? 0 : sizeMap[i], useMap[i]);
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
}

void *
MemAllocate (DataType *type, int size)
{
    DataType	**data;

    if (size == 0)
	abort ();
    data = newObject (size);
    memset ((void *) data, '\0', size);
    *data = type;
    return data;
}

void
MemReference (void *object)
{
    DataType	*type;

    if (!object)
	return;
    if (!setReference (object))
    {
	type = TYPE(object);
	if (type && type->Mark)
	    (*type->Mark) (object);
    }
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

