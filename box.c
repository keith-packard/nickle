/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"value.h"

static void
BoxMark (void *object)
{
    BoxPtr	box = object;
    BoxElement	*elements;
    int		i;

    elements = BoxElements(box);
    for (i = 0; i < box->nvalues; i++)
    {
	MemReference (elements[i].value);
	MemReference (elements[i].type);
    }
}

DataType BoxType = { BoxMark, 0 };

BoxPtr
NewBox (Bool constant, int nvalues)
{
    ENTER ();
    BoxPtr  box;
    int	    i;

    box = ALLOCATE (&BoxType, sizeof (Box) + nvalues * sizeof (BoxElement));
    box->constant = constant;
    box->nvalues = nvalues;
    for (i = 0; i < nvalues; i++)
    {
	BoxType(box, i) = typesPoly;
	BoxValue(box, i) = Zero;
    }	
    RETURN (box);
}

BoxPtr
NewTypedBox (Bool constant, BoxTypesPtr bt)
{
    ENTER ();
    BoxPtr  box;
    int	    i;
    Types   *t;

    box = NewBox (constant, bt->count);
    for (i = 0; i < bt->count; i++)
    {
	t = BoxTypesValue (bt, i);
	BoxType (box, i) = t;
	BoxValue (box, i) = Default (t);
    }
    RETURN (box);
}

static void MarkBoxTypes (void *object)
{
    /* No referenced objects */
}

DataType    BoxTypesType = { MarkBoxTypes, 0 };

#define BT_INCR	4

BoxTypesPtr
NewBoxTypes (int size)
{
    ENTER ();
    BoxTypesPtr    bt;

    bt = ALLOCATE (&BoxTypesType, sizeof (BoxTypes) + size * sizeof (Type));
    bt->size = size;
    bt->count = 0;
    RETURN (bt);
}

int
AddBoxTypes (BoxTypesPtr *btp, Types *t)
{
    ENTER ();
    BoxTypesPtr  bt, new;
    int		count, size;
    int		position;
    
    bt = *btp;
    if (!bt)
    {
	count = 0;
	size = 0;
    }
    else
    {
	count = bt->count;
	size = bt->size;
    }
    if (count == size)
    {
	size = size + BT_INCR;
	new = NewBoxTypes (size);
	if (count)
	{
	    memcpy (BoxTypesElements (new), BoxTypesElements (bt),
		    count * sizeof (Type));
	}
	new->size = size;
	new->count = count;
	*btp = new;
	bt = new;
    }
    position = bt->count++;
    BoxTypesValue(bt,position) = t;
    EXIT ();
    return position;
}
