/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#ifndef _STACK_H_
#define _STACK_H_
#define STACK_ENTS_PER_CHUNK	125

typedef void		*StackElement;
typedef StackElement	*StackPointer;

typedef struct _StackChunk {
    DataType		*type;
    struct _StackChunk	*previous;
    StackPointer	stackPointer;
    StackElement	elements[STACK_ENTS_PER_CHUNK];
} StackChunk;

typedef struct _Stack {
    DataType		*type;
    StackChunk		*current;
    StackChunk		*save;
    StackElement	temp;
} StackObject;

extern StackObject  *StackCreate (void);
extern StackObject  *StackCopy (StackObject *stack);
extern StackElement StackPush (StackObject *stack, StackElement object);
extern StackElement StackPop (StackObject *stack);
extern void	    StackDrop (StackObject *stack, int i);
extern void	    StackReset (StackObject *stack, StackPointer stackPointer);
extern int	    StackCheck (StackObject *stack, StackElement object);
extern StackElement StackReturn (StackObject *stack, StackPointer stackPointer, StackElement object);
extern StackElement StackElt (StackObject *stack, int i);

#define STACK_MAX(s)	((s)->current->elements + STACK_ENTS_PER_CHUNK)
#define STACK_MIN(s)	((s)->current->elements)
#define STACK_TOP(s)	((s)->current->stackPointer)

#define STACK_POP(s)	    ((STACK_TOP(s) == STACK_MAX(s)) ? \
			     StackPop (s) : *STACK_TOP(s)++)

#define STACK_DROP(s,i)	    ((STACK_TOP(s) + (i) <= STACK_MAX(s)) ? \
			     ((STACK_TOP(s) += (i)), 0) : (StackDrop(s, i), 0))

#define STACK_RESET(s,sp)   ((!(STACK_TOP(s) <= (sp) && (sp) <= STACK_MAX(s))) ? \
			     (StackReset ((s), (sp)), 0) : ((STACK_TOP(s) = (sp)), 0))

#define STACK_ELT(s,i)	((STACK_TOP(s) + (i) < STACK_MAX(s)) ? \
			 STACK_TOP(s)[i] : StackElt(s,i))

#if 0
/*
 * Can't work -- o gets evaluated after the stack overflow check, 
 * if o also uses the stack, this will break
 */
#define STACK_PUSH(s,o)	    ((STACK_TOP(s) == STACK_MIN(s)) ? \
			     StackPush ((s), (o)) : (*--STACK_TOP(s) = (o)))
#endif

#ifdef HAVE_C_INLINE
static inline StackElement
StackPushInline(StackObject *s, StackElement o)
{
    if (STACK_TOP(s) == STACK_MIN(s))
	return StackPush (s, o);
    return *--STACK_TOP(s) = o;
}
#define STACK_PUSH(s,o) StackPushInline(s,o)
static inline StackElement
StackReturnInline(StackObject *s, StackPointer sp, StackElement o)
{
    STACK_RESET(s, sp);
    if (STACK_TOP(s) == STACK_MIN(s))
	return StackPush (s, o);
    return *--STACK_TOP(s) = o;
}
#define STACK_RETURN(s,sp,o) StackReturnInline(s,sp,o)
#endif

#ifndef STACK_PUSH
#define STACK_PUSH(s,o)	StackPush(s,o)
#endif
#ifndef STACK_POP
#define STACK_POP(s) StackPop(s)
#endif
#ifndef STACK_DROP
#define STACK_DROP(s,i)	StackDrop(s,i)
#endif
#ifndef STACK_RESET
#define STACK_RESET(s,sp) StackReset(s,sp)
#endif
#ifndef STACK_ELT
#define STACK_ELT(s,i) StackElt(s,i)
#endif
#ifndef STACK_RETURN
#define STACK_RETURN(s,sp,o)	StackReturn(s,sp,o)
#endif

#endif /* _STACK_H_ */
