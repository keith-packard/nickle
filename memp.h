/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * mem.h
 *
 * definitions for the memory manager
 */

struct bfree {
	DataType	*type;
	struct bfree	*next;
};

/* make sure we can store doubles in blocks */

union block_round {
    struct block    b;
    double	    round;
};

# define MINHUNK	(sizeof (struct bfree))
# define NUMSIZES	12
# define HUNKSIZE(i)	(MINHUNK << (i))
# define MAXHUNK	HUNKSIZE(NUMSIZES-1)

# define TYPE(o)	(*((DataType **) (o)))
# define HEADSIZE	(sizeof (union block_round))
# define MINBLOCKSIZE	(MAXHUNK + MINHUNK + HEADSIZE)
# define GOODBLOCKSIZE	(0x7fc)
# define BLOCKSIZE	(GOODBLOCKSIZE < MINBLOCKSIZE ? \
			 MINBLOCKSIZE : GOODBLOCKSIZE)

# define GARBAGETIME	1000

# define BITSPERCH		(8)
# define NUMINBLOCK(size) 	(((BLOCKSIZE - HEADSIZE) * \
				  BITSPERCH) / (1 + BITSPERCH * (size)))
# define BITMAPSIZE(size)	((NUMINBLOCK(size) + (BITSPERCH-1)) / BITSPERCH)

