/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#ifndef _MEM_H_
#define _MEM_H_

#undef MEM_TRACE

#ifndef MEM_TRACE
typedef const struct _DataType DataType;
#else
typedef struct _DataType DataType;
#endif

struct _DataType {
    void    (*Mark) (void *);
    void    (*Free) (void *);
    char    *name;
#ifdef MEM_TRACE
    int	    added;
    int	    total;
    int	    active;
    long long	    total_bytes;
    long long	    active_bytes;
    DataType	*next;
#endif
};

struct bfree {
	DataType	*type;
	struct bfree	*next;
};

/* make sure we can store doubles in blocks */

#include "avl.h"

union block_round {
    struct block    b;
    double	    round;
};

# define MINHUNK	(sizeof (struct bfree))
# define NUMSIZES	12
# define HUNKSIZE(i)	(MINHUNK << (i))
# define MAXHUNK	HUNKSIZE(NUMSIZES-1)

extern void	MemInitialize (void);
#ifndef HAVE_C_INLINE
extern void	*MemAllocate (DataType *type, int size);
#endif
extern void	MemFree (void *object, int size);
extern void	*MemAllocateHuge (DataType *type, int size);
extern void	*MemHunkMore (int sizeIndex);
extern void	MemReference (void *object);
extern int	MemReferenceNoRecurse (void *object);
extern DataType	*MemType (void *object);
extern void	MemAddRoot (void *object);
extern void	MemCollect (void);
extern void	MemCheckPointer (void *base, void *address, int size);
#ifdef MEM_TRACE
extern void	MemAddType (DataType *type);
extern void	MemActiveDump (void);
#endif

extern void	debug (char *, ...);
extern void	panic (char *, ...);

extern int	GCdebug;

#include	"stack.h"

extern	StackObject *MemStack;

extern void	    *TemporaryData;

extern struct bfree *freeList[NUMSIZES];

#define REFERENCE(o)	    STACK_PUSH(MemStack, (o))
#define ENTER()		    StackPointer    __stackPointer = STACK_TOP(MemStack)
#define ALLOCATE(type,size) REFERENCE(MemAllocate(type,size))
#define EXIT()		    STACK_RESET(MemStack, __stackPointer)
#define RETURN(r)	    return (STACK_RETURN (MemStack, __stackPointer, (r)))
#define NOREFRETURN(r)	    return (EXIT(), (r))

#if defined(HAVE_C_INLINE) || defined(MEM_NEED_ALLOCATE)

/*
 * Allocator entry point
 */

#ifdef HAVE_C_INLINE
static inline void *
#else
void *
#endif
MemAllocate (DataType *type, int size)
{
    int	sizeIndex = 0;
    struct bfree    *new;
    
#if NUMSIZES > 16
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
	return MemAllocateHuge (type, size);
    
    new = freeList[sizeIndex];
    if (!new)
	new = MemHunkMore (sizeIndex);
    freeList[sizeIndex] = new->next;
    if (sizeIndex >= 6)
	memset (new, '\0', size);
    new->type = type;
    return (void *) new;
}
#endif

#endif /* _MEM_H_ */
