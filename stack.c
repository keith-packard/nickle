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

static DataType stackType = { stackMark, 0, "stackType" };

static DataType stackChunkType = { 0, 0, "stackChunkType" };

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
    
    chunk->previous = stack->current;
    stack->current = chunk;
    STACK_TOP(stack) = CHUNK_MAX(chunk);
}

StackObject *
StackCreate (void)
{
    StackObject	*stack;

    stack = MemAllocate (&stackType, sizeof (StackObject));
    stack->current = 0;
    stack->save = 0;
    stack->temp = 0;
    stack->stackPointer = 0;
    TemporaryData = stack;
    addChunk (stack);
    TemporaryData = 0;
    return stack;
}

void *
StackPush (StackObject *stack, StackElement object)
{
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
	STACK_TOP(stack) = previous->elements;
    }
    return *STACK_TOP(stack)++;
}

void
StackDrop (StackObject *stack, int i)
{
    int		this;
    StackChunk	*previous;
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
	STACK_TOP(stack) = CHUNK_MIN(previous);
    }
}

void
StackReset (StackObject *stack, StackPointer stackPointer)
{
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
	STACK_TOP(stack) = CHUNK_MIN(previous);
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
    StackPointer    stackPointer;

    chunk = stack->current;
    stackPointer = STACK_TOP(stack);
    while (stackPointer + i >= CHUNK_MAX(chunk))
    {
	i -= CHUNK_MAX(chunk) - stackPointer;
	chunk = chunk->previous;
	if (!chunk)
	    panic ("StackElt underflow");
	stackPointer = CHUNK_MIN(chunk);
    }
    return stackPointer[i];
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
	
	/*
	 * Link into chain
	 */
	*prev = nchunk;
	prev = &nchunk->previous;
	chunk = chunk->previous;
	nchunk = 0;
    }
    STACK_TOP(new) = new->current->elements + (STACK_TOP(stack) - stack->current->elements);
    RETURN (new);
}

static void
stackMark (void *object)
{
    StackObject	    *stack = object;
    StackChunk	    *chunk;
    StackPointer    stackPointer;

    MemReference (stack->temp);
    MemReference (stack->save);
    chunk = stack->current;
    if (chunk)
    {
	stackPointer = STACK_TOP(stack);
	for (;;)
	{
	    MemReference (chunk);
	    while (stackPointer != CHUNK_MAX(chunk))
		MemReference (*stackPointer++);
	    chunk = chunk->previous;
	    if (!chunk)
		break;
	    stackPointer = CHUNK_MIN(chunk);
	}
    }
}
