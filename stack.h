/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#ifndef _STACK_H_
#define _STACK_H_
#include <stddef.h>

#define STACK_ENTS_PER_CHUNK 126

typedef void         *StackElement;
typedef StackElement *StackPointer;

typedef struct _StackChunk {
    DataType           *type;
    struct _StackChunk *previous;
    StackElement        elements[STACK_ENTS_PER_CHUNK];
} StackChunk;

typedef struct _Stack {
    DataType    *type;
    StackPointer stackPointer;
    StackChunk  *current;
    StackChunk  *save;
    StackElement temp;
} StackObject;

extern StackObject *StackCreate(void);
extern StackObject *StackCopy(StackObject *stack);
extern StackElement StackPush(StackObject *stack, StackElement object);
extern StackElement StackPop(StackObject *stack);
extern void         StackDrop(StackObject *stack, ptrdiff_t i);
extern void         StackReset(StackObject *stack, StackPointer stackPointer);
extern StackElement StackReturn(StackObject *stack, StackPointer stackPointer, StackElement object);
extern StackElement StackElt(StackObject *stack, ptrdiff_t i);

#define CHUNK_MAX(c) ((c)->elements + STACK_ENTS_PER_CHUNK)
#define CHUNK_MIN(c) ((c)->elements)
#define STACK_MAX(s) (CHUNK_MAX((s)->current))
#define STACK_MIN(s) (CHUNK_MIN((s)->current))
#define STACK_TOP(s) ((s)->stackPointer)

/*
 * Fast paths for operations within the current chunk, falling back to
 * the functions in stack.c when they cross chunks
 */
static inline StackElement
StackPopInline(StackObject *s)
{
    if (STACK_TOP(s) == STACK_MAX(s))
        return StackPop(s);
    return *STACK_TOP(s)++;
}
#define STACK_POP(s) StackPopInline(s)

static inline void
StackDropInline(StackObject *s, ptrdiff_t i)
{
    if (STACK_TOP(s) + i <= STACK_MAX(s))
        STACK_TOP(s) += i;
    else
        StackDrop(s, i);
}
#define STACK_DROP(s, i) StackDropInline(s, i)

static inline void
StackResetInline(StackObject *s, StackPointer sp)
{
    if (STACK_TOP(s) == sp)
        return;
    if (STACK_TOP(s) <= sp && sp <= STACK_MAX(s))
        STACK_TOP(s) = sp;
    else
        StackReset(s, sp);
}
#define STACK_RESET(s, sp) StackResetInline(s, sp)

static inline StackElement
StackEltInline(StackObject *s, ptrdiff_t i)
{
    if (STACK_TOP(s) + i < STACK_MAX(s))
        return STACK_TOP(s)[i];
    return StackElt(s, i);
}
#define STACK_ELT(s, i) StackEltInline(s, i)

#if 0
#define STACK_VALID(s)                                                  \
    ((!(s)->stackPointer && !(s)->current)                              \
     || (STACK_MIN(s) <= STACK_TOP(s) && STACK_TOP(s) <= STACK_MAX(s)))

void
panic (char *, ...);

#define STACK_ASSERT(s)           \
    if (!STACK_VALID(s))          \
        panic("invalid stack\n");
#define STACK_CHUNK_ASSERT(c) assert((c)->type == &stackChunkType)
#else
#define STACK_ASSERT(s)
#define STACK_CHUNK_ASSERT(c)
#endif

#if 0
/*
 * Can't work -- o gets evaluated after the stack overflow check, 
 * if o also uses the stack, this will break
 */
#define STACK_PUSH(s, o)                                                             \
    ((STACK_TOP(s) == STACK_MIN(s)) ? StackPush((s), (o)) : (*--STACK_TOP(s) = (o)))
#endif

static inline StackElement
StackPushInline(StackObject *s, StackElement o)
{
    STACK_ASSERT(s);
    if (STACK_TOP(s) == STACK_MIN(s))
        return StackPush(s, o);
    return *--STACK_TOP(s) = o;
}
#define STACK_PUSH(s, o) StackPushInline(s, o)
static inline StackElement
StackReturnInline(StackObject *s, StackPointer sp, StackElement o)
{
    STACK_ASSERT(s);
    STACK_RESET(s, sp);
    STACK_ASSERT(s);
    if (STACK_TOP(s) == STACK_MIN(s))
        return StackPush(s, o);
    return *--STACK_TOP(s) = o;
}
#define STACK_RETURN(s, sp, o) StackReturnInline(s, sp, o)

#endif /* _STACK_H_ */
