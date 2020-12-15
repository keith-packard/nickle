/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<assert.h>

#include	"nickle-config.h"
#include	"mem.h"
#include	"stack.h"

static void	stackMark (void *);

DataType stackType = { stackMark, 0, "stackType" };

DataType stackChunkType = { 0, 0, "stackChunkType" };

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
    STACK_ASSERT (stack);
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
    STACK_ASSERT (stack);
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
	    panic ("Stack underflow\n");
	STACK_TOP(stack) = previous->elements;
    }
    return *STACK_TOP(stack)++;
}

void
StackDrop (StackObject *stack, int i)
{
    int		this;
    StackChunk	*previous;
    STACK_ASSERT (stack);
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
	    panic ("Stack underflow\n");
	STACK_TOP(stack) = CHUNK_MIN(previous);
    }
    STACK_ASSERT (stack);
}

void
StackReset (StackObject *stack, StackPointer stackPointer)
{
    STACK_ASSERT (stack);
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
	    panic ("Stack underflow\n");
	STACK_TOP(stack) = CHUNK_MIN(previous);
    }
    STACK_TOP(stack) = stackPointer;
    STACK_ASSERT (stack);
}

StackElement
StackReturn (StackObject *stack, StackPointer stackPointer, StackElement object)
{
    STACK_ASSERT (stack);
    STACK_RESET(stack, stackPointer);
    return STACK_PUSH(stack,object);
}

StackElement
StackElt (StackObject *stack, int i)
{
    StackChunk	    *chunk;
    StackPointer    stackPointer;

    STACK_ASSERT (stack);
    chunk = stack->current;
    stackPointer = STACK_TOP(stack);
    while (stackPointer + i >= CHUNK_MAX(chunk))
    {
	i -= CHUNK_MAX(chunk) - stackPointer;
	chunk = chunk->previous;
	if (!chunk)
	    panic ("StackElt underflow\n");
	stackPointer = CHUNK_MIN(chunk);
    }
    return stackPointer[i];
}

StackObject *
StackCopy (StackObject *stack)
{
    StackObject	*new;
    StackChunk	*chunk, *nchunk, **prev;

    STACK_ASSERT (stack);
    new = StackCreate ();
    REFERENCE (new);
    chunk = stack->current;
    nchunk = new->current;
    prev = &new->current;
    while (chunk)
    {
	STACK_ASSERT (new);
	STACK_ASSERT (stack);
	if (!nchunk)
	    nchunk = MemAllocate (&stackChunkType, sizeof (StackChunk));
	else
	    STACK_TOP(new) = (new->current->elements + 
			      (STACK_TOP(stack) - stack->current->elements));
	    
	/*
	 * Copy stack data and fix stack pointer
	 */
	*nchunk = *chunk;
	STACK_CHUNK_ASSERT(chunk->type == &stackChunkType);
	
	/*
	 * Link into chain
	 */
	*prev = nchunk;
	prev = &nchunk->previous;
	*prev = 0;
	chunk = chunk->previous;
	nchunk = 0;
    }
    STACK_ASSERT (new);
    return new;
}

static void
stackMark (void *object)
{
    StackObject	    *stack = object;
    StackChunk	    *chunk;
    StackPointer    stackPointer;

    STACK_ASSERT (stack);
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
