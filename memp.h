/* $Header$ */

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * mem.h
 *
 * definitions for the memory manager
 */

# define TYPE(o)	(*((DataType **) (o)))
# define HEADSIZE	(sizeof (union block_round))
# define MINBLOCKSIZE	(MAXHUNK + MINHUNK + HEADSIZE)
# define GOODBLOCKSIZE	(0x2000)
# define BLOCKSIZE	(GOODBLOCKSIZE < MINBLOCKSIZE ? \
			 MINBLOCKSIZE : GOODBLOCKSIZE)
# define DATASIZE	(BLOCKSIZE - HEADSIZE)
# define NUMHUNK(i)	((i) >= NUMSIZES ? 1 : (DATASIZE / HUNKSIZE(i)))

# define HUNKS(b)	((unsigned char *) (b) + HEADSIZE)

# define GARBAGETIME	1000
