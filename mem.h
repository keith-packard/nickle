/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#ifndef _MEM_H_
#define _MEM_H_

#ifdef HAVE_STRING_H
#include	<string.h>
#endif
#ifdef HAVE_STRINGS_H
#include	<strings.h>
#endif

#ifdef MEM_TRACE
typedef struct _DataType DataType;
#else
typedef const struct _DataType DataType;
#endif

#if LOCAL_BUILD
#include	"stack.h"
#else
#include	<nickle/stack.h>
#endif

struct _DataType {
    void    (*Mark) (void *);
    int	    (*Free) (void *);
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

typedef unsigned long PtrInt;

struct block {
    struct block    *next;
    int		    sizeIndex;
};

/* make sure we can store doubles in blocks */

union block_round {
    struct block    b;
    double	    round;
};

# define MINHUNK	(sizeof (struct bfree))
# define NUMSIZES	12
# define HUNKSIZE(i)	(MINHUNK << (i))
# define MAXHUNK	HUNKSIZE(NUMSIZES-1)
# define HEADSIZE	(sizeof (union block_round))
# define HUNKS(b)	((unsigned char *) (b) + HEADSIZE)

struct bfree *
MemAllocateHunk (int sizeIndex);

struct bfree *
MemAllocateHuge (int size);

void
*MemAllocate (DataType *type, int size);

void
*MemAllocateRef (DataType *type, int size);

#ifdef MEM_TRACE
void
MemAddType (DataType *type);

void
MemActiveDump (void);
#endif

void
MemInitialize (void);

void
MemAddRoot (void *object);

void
MemReference (void *object);

int
MemReferenceNoRecurse (void *object);

void
MemCollect (void);
    
/*
 * These are used by the mem system and defined externally
 */

void
debug (char *, ...);

void
panic (char *, ...);

extern StackObject  *MemStack;
extern void	    *TemporaryData;

extern struct bfree *freeList[NUMSIZES];

#define REFERENCE(o)	    STACK_PUSH(MemStack, (o))
#define ENTER()		    StackPointer    __stackPointer = STACK_TOP(MemStack)
#define ALLOCATE(type,size) MemAllocateRef(type,size)
#define EXIT()		    STACK_RESET(MemStack, __stackPointer)
#define RETURN(r)	    return (STACK_RETURN (MemStack, __stackPointer, (r)))
#define NOREFRETURN(r)	    return (EXIT(), (r))

#endif /* _MEM_H_ */
