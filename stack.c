/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"mem.h"
#include	"stack.h"

static void	stackMark (void *);

static DataType stackType = {
    stackMark, 0
};

static void	stackChunkMark (void *);

static DataType stackChunkType = {
    stackChunkMark, 0
};

static void
addChunk (StackObject *stack)
{
    StackChunk	*chunk;
    
    if (stack->save)
    {
	chunk = stack->save;
	stack->save = 0;
    }
    else
	chunk = MemAllocate (&stackChunkType, sizeof (StackChunk));
    
    chunk->stackPointer = &chunk->elements[STACK_ENTS_PER_CHUNK];
    chunk->previous = stack->current;
    stack->current = chunk;
}

static void
StackValidate (StackObject *stack)
{
    StackChunk	*chunk;

    for (chunk = stack->current; chunk; chunk = chunk->previous)
    {
	if (!(chunk->elements <= chunk->stackPointer &&
	      chunk->stackPointer <= chunk->elements + STACK_ENTS_PER_CHUNK))
	{
	    panic ("invalid stack");
	}
    }
}

StackObject *
StackCreate (void)
{
    StackObject	*stack;

    stack = MemAllocate (&stackType, sizeof (StackObject));
    TemporaryData = stack;
    addChunk (stack);
    TemporaryData = 0;
    return stack;
}

void *
StackPush (StackObject *stack, StackElement object)
{
#if 0
    StackValidate (stack);
#endif
    if (STACK_TOP(stack) == STACK_MIN(stack))
    {
	stack->temp = object;
	addChunk (stack);
	stack->temp = 0;
    }
    return *--STACK_TOP(stack) = object;
}

void *
StackPop (StackObject *stack)
{
#if 0
    StackValidate (stack);
#endif
    if (STACK_TOP(stack) == STACK_MAX(stack))
    {
	StackChunk  *previous = stack->current->previous;
	
	if (!stack->save)
	{
	    stack->save = stack->current;
	    stack->save->previous = 0;
	}
	stack->current = previous;
	if (!stack->current)
	    panic ("Stack underflow");
    }
    return *STACK_TOP(stack)++;
}

void
StackDrop (StackObject *stack, int i)
{
    int		this;
    StackChunk	*previous;
#if 0
    StackValidate (stack);
#endif
    while (i)
    {
	this = STACK_MAX(stack) - STACK_TOP(stack);
	if (this >= i)
	{
	    STACK_TOP(stack) += i;
	    break;
	}
	i -= this;
	previous = stack->current->previous;

	if (!stack->save)
	{
	    stack->save = stack->current;
	    stack->save->previous = 0;
	}
	stack->current = previous;
	if (!stack->current)
	    panic ("Stack underflow");
    }
}

void
StackReset (StackObject *stack, StackPointer stackPointer)
{
#if 0
    StackValidate (stack);
#endif
    while (!(STACK_TOP(stack) <= stackPointer && stackPointer <= STACK_MAX(stack)))
    {
	StackChunk  *previous = stack->current->previous;
	
	if (!stack->save)
	{
	    stack->save = stack->current;
	    stack->save->previous = 0;
	}
	stack->current = previous;
	if (!stack->current)
	    panic ("Stack underflow");
    }
    STACK_TOP(stack) = stackPointer;
}

StackElement
StackReturn (StackObject *stack, StackPointer stackPointer, StackElement object)
{
    STACK_RESET(stack, stackPointer);
    return STACK_PUSH(stack,object);
}

StackElement
StackElt (StackObject *stack, int i)
{
    StackChunk	    *chunk;

    chunk = stack->current;
    while (!(chunk->stackPointer + i < chunk->elements + STACK_ENTS_PER_CHUNK))
    {
	i -= (chunk->elements + STACK_ENTS_PER_CHUNK) - chunk->stackPointer;
	chunk = chunk->previous;
	if (!chunk)
	    panic ("StackElt underflow");
    }
    return chunk->stackPointer[i];
}

int
StackCheck (StackObject *stack, StackElement object)
{
    StackChunk	    *chunk;
    StackPointer    stackPointer;

    StackValidate (stack);
    for (chunk = stack->current; chunk; chunk = chunk->previous)
    {
	for (stackPointer = chunk->stackPointer; 
	     stackPointer != chunk->elements + STACK_ENTS_PER_CHUNK;
	     stackPointer++)
	{
	    if (*stackPointer == object)
		return 1;
	}
    }
    return 0;
}

StackObject *
StackCopy (StackObject *stack)
{
    ENTER ();
    StackObject	*new;
    StackChunk	*chunk, *nchunk, **prev;

    new = StackCreate ();
    chunk = stack->current;
    nchunk = new->current;
    prev = &new->current;
    while (chunk)
    {
	if (!nchunk)
	    nchunk = MemAllocate (&stackChunkType, sizeof (StackChunk));
	/*
	 * Copy stack data and fix stack pointer
	 */
	*nchunk = *chunk;
	nchunk->stackPointer = nchunk->elements + (chunk->stackPointer - chunk->elements);
	
	/*
	 * Link into chain
	 */
	*prev = nchunk;
	prev = &nchunk->previous;
	chunk = chunk->previous;
	nchunk = 0;
    }
    RETURN (new);
}

static void
stackMark (void *object)
{
    StackObject	*stack = object;

    MemReference (stack->temp);
    MemReference (stack->current);
    MemReference (stack->save);
}

static void
stackChunkMark (void *object)
{
    StackChunk	    *chunk = object;
    StackPointer    stackPointer;

    stackPointer = chunk->stackPointer;
    while (stackPointer != chunk->elements + STACK_ENTS_PER_CHUNK)
	MemReference (*stackPointer++);
    MemReference (chunk->previous);
}
