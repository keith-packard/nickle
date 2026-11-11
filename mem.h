/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#ifndef _MEM_H_
#define _MEM_H_

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef MEM_TRACE
typedef struct _DataType DataType;
#else
typedef const struct _DataType DataType;
#endif

#if LOCAL_BUILD
#include "stack.h"
#else
#include <nickle/stack.h>
#endif

extern DataType stackType, stackChunkType;

/*
 * Annotation, after the declarator, for functions which never return
 */
#if defined(__GNUC__)
#define __NICKLE_NORETURN __attribute__((noreturn))
#else
#define __NICKLE_NORETURN
#endif

/*
 * Annotation, after the declarator, for functions taking printf formats
 */
#if defined(__GNUC__)
#define __NICKLE_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define __NICKLE_PRINTF(fmt, args)
#endif

struct _DataType {
    void  (*Mark)(void *);
    int   (*Free)(void *);
    char *name;
#ifdef MEM_TRACE
    int       added;
    int       total;
    int       active;
    size_t    total_bytes;
    size_t    active_bytes;
    DataType *next;
#endif
};

struct bfree {
    DataType     *type;
    struct bfree *next;
};

typedef unsigned long PtrInt;

struct block {
    struct block *next;
    size_t        sizeIndex; /* byte size for huge blocks */
};

/* make sure we can store doubles in blocks */

union block_round {
    struct block b;
    double       round;
};

#define MINHUNK     (sizeof(struct bfree))
#define NUMSIZES    12
#define HUNKSIZE(i) (MINHUNK << (i))
#define MAXHUNK     HUNKSIZE(NUMSIZES - 1)
#define HEADSIZE    (sizeof(union block_round))
#define HUNKS(b)    ((unsigned char *)(b) + HEADSIZE)

struct bfree *MemAllocateHunk(size_t sizeIndex);

struct bfree *MemAllocateHuge(size_t size);

void         *MemAllocate(DataType *type, size_t size);

void         *MemAllocateRef(DataType *type, size_t size);

#ifdef MEM_TRACE
void MemAddType(DataType *type);

void MemActiveDump(void);
#endif

void                 MemInitialize(void);

void                 MemAddRoot(void *object);

void                 MemReference(void *object);

int                  MemReferenceNoRecurse(void *object);

void                 MemCollect(void);

/*
 * These are used by the mem system and defined externally
 */

void                 debug(char *, ...) __NICKLE_PRINTF(1, 2);

void                 panic(char *, ...) __NICKLE_NORETURN __NICKLE_PRINTF(1, 2);

void                 MemExhausted(void) __NICKLE_NORETURN;

extern StackObject  *MemStack;
extern void         *TemporaryData;

extern struct bfree *freeList[NUMSIZES];

/*
 * Values allocated in a function stay referenced on MemStack, safe from
 * collection, until the function releases them. MemMark records the
 * stack position, MemRelease returns to it, and MemReturn returns to
 * it and then references 'v' again, so a returned value stays safe in
 * the caller's scope.
 */
static inline StackPointer
MemMark(void)
{
    return STACK_TOP(MemStack);
}

static inline void
MemRelease(StackPointer mark)
{
    STACK_RESET(MemStack, mark);
}

static inline void *
MemReturn(StackPointer mark, void *v)
{
    return STACK_RETURN(MemStack, mark, v);
}

#define REFERENCE(o)         STACK_PUSH(MemStack, (o))
#define ENTER()              StackPointer __stackPointer = MemMark()
#define ALLOCATE(type, size) MemAllocateRef(type, size)
#define EXIT()               MemRelease(__stackPointer)
#define RETURN(r)            return MemReturn(__stackPointer, (r))
/*
 * Return 'r' and store 'o' through 'op', keeping both referenced in the
 * caller's scope. The stack always has room to push back two values
 * after a reset.
 */
#define RETURN2(r, o, op) return (EXIT(), REFERENCE(r), REFERENCE(o), *(op) = (o), (r))

#endif /* _MEM_H_ */
