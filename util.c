/* $Header$ */
/*
 * This program is Copyright (C) 1988 by Keith Packard.  NICK is provided to
 * you without charge, and with no warranty.  You may give away copies of
 * NICK, including source, provided that this notice is included in all the
 * files.
 */
/*
 *	util.c
 *
 *	general purpose utilities
 */

# include	<math.h>
# include	"nick.h"

#ifdef notdef
double
dist (x0, y0, x1, y1)
double	x0, y0, x1, y1;
{
	register double	tx, ty;
	
	tx = x0 - x1;
	ty = y0 - y1;
	return sqrt (tx*tx + ty*ty);
}
#endif

void *
AllocateTemp (int size)
{
    DataType	**b;
    
    b = ALLOCATE (0, sizeof (DataType *) + size);
    return b + 1;
}
