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

#include    "stack.h"

extern	StackObject *MemStack;

extern void	    *TemporaryData;

#define REFERENCE(o)	    STACK_PUSH(MemStack, (o))
#define ENTER()		    StackPointer    __stackPointer = STACK_TOP(MemStack)
#define ALLOCATE(type,size) REFERENCE(MemAllocate(type,size))
#define EXIT()		    STACK_RESET(MemStack, __stackPointer)
#define RETURN(r)	    return (STACK_RETURN (MemStack, __stackPointer, (r)))
#define NOREFRETURN(r)	    return (EXIT(), (r))

#endif /* _MEM_H_ */
