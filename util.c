/* $Header$ */

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	util.c
 *
 *	general purpose utilities
 */

#include	"nickle.h"

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

DataType    TempType = { 0, 0, "TempType" };

void *
AllocateTemp (int size)
{
    DataType	**b;
    
    b = ALLOCATE (&TempType, sizeof (DataType *) + size);
    return b + 1;
}



#include	<stdarg.h>
#include	<stdio.h>

#ifdef HAVE_VPRINTF

/*
 * Currently vfprintf() is required.  It would
 * be easy to do a _doprnt() version if necessary,
 * and it would certainly be possible to develop
 * non-varargs versions of these.  Contributed code welcome.
 */

void
debug (char *format, ...)
{
    va_list	ap;

    va_start (ap, format);
    vfprintf (stderr, format, ap);
    va_end (ap);
}

void
panic (char *format, ...)
{
    va_list	ap;

    va_start (ap, format);
    vfprintf (stderr, format, ap);
    va_end (ap);
    abort ();
}

#endif
