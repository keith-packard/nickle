/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#ifndef _MEMP_H_
#define _MEMP_H_

/*
 * memp.h
 *
 * definitions for the memory manager
 */

# define TYPE(o)	    (*((DataType **) (o)))
# define MINBLOCKSIZE	    (MAXHUNK + MINHUNK + HEADSIZE)
# define GOODBLOCKSIZE	    (0x2000)
# define BLOCKSIZE	    (GOODBLOCKSIZE < MINBLOCKSIZE ? \
			     MINBLOCKSIZE : GOODBLOCKSIZE)
# define DATASIZE	    (BLOCKSIZE - HEADSIZE)
# define NUMHUNK(i)	    (DATASIZE / HUNKSIZE(i))
# define NUMHUNK_ALL(i)	    ((i) >= NUMSIZES ? 1 : NUMHUNK(i))
# define HUNKSIZE_ALL(i)    ((i) >= NUMSIZES ? (i) : HUNKSIZE(i))

# define GARBAGETIME	1000

#if HAVE_STDINT_H
#include	<stdint.h>
#define PtrToInt(p)	((int) (intptr_t) (p))
typedef intptr_t	IntPtr;
#else
#define PtrToInt(p)	((int) (p))
typedef int		IntPtr;
#endif

/*
 * Reference bits are stored in the low bit of the DataType pointer
 * which exists at the head of each object
 */

#define fetchRefInt(a)	    ((IntPtr) (*(DataType **) (a)))
#define storeRefInt(a,v)    ((*(DataType **) (a)) = (DataType *) (v))
#define isReferenced(a)	    (fetchRefInt(a) & 1)
#define clrReference(a)	    (storeRefInt(a,fetchRefInt(a) & ~1))
#define setReference(a)	    (storeRefInt(a,fetchRefInt(a) | 1))

#endif /* _MEMP_H_ */
