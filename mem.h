/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#ifndef _MEM_H_
#define _MEM_H_
typedef struct {
    void    (*Mark) (void *);
    void    (*Free) (void *);
} DataType;

extern void	MemInitialize (void);
extern void	*MemAllocate (DataType *type, int size);
extern void	MemReference (void *object);
extern DataType	*MemType (void *object);
extern void	MemAddRoot (void *object);
extern void	MemCollect (void);

extern void	debug (char *, ...);
extern void	panic (char *, ...);

extern int	GCdebug;

#include	"stack.h"

extern	StackObject *MemStack;

extern void	    *TemporaryData;

#define REFERENCE(o)	    STACK_PUSH(MemStack, (o))
#define ENTER()		    StackPointer    __stackPointer = STACK_TOP(MemStack)
#define ALLOCATE(type,size) REFERENCE(MemAllocate(type,size))
#define EXIT()		    STACK_RESET(MemStack, __stackPointer)
#define RETURN(r)	    return (STACK_RETURN (MemStack, __stackPointer, (r)))
#define NOREFRETURN(r)	    return (EXIT(), (r))

#endif /* _MEM_H_ */
