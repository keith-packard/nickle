/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

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
NewBox (Bool constant, Bool array, int nvalues)
{
    ENTER ();
    BoxPtr  box;
    int	    i;

    box = ALLOCATE (&BoxType, sizeof (Box) + nvalues * sizeof (BoxElement));
    box->constant = constant;
    box->array = array;
    box->nvalues = nvalues;
    for (i = 0; i < nvalues; i++)
    {
	BoxType(box, i) = typesPoly;
	BoxValueSet(box, i, 0);
    }
    RETURN (box);
}

BoxPtr
NewTypedBox (Bool array, BoxTypesPtr bt)
{
    ENTER ();
    BoxPtr  box;
    int	    i;

    box = ALLOCATE (&BoxType, sizeof (Box) + bt->count * sizeof (BoxElement));
    box->constant = False;
    box->array = array;
    box->nvalues = bt->count;
    for (i = 0; i < bt->count; i++)
    {
	BoxType (box, i) = BoxTypesValue (bt, i);
	BoxValueSet (box, i, 0);
    }
    RETURN (box);
}

#ifndef HAVE_C_INLINE
Value
BoxValue (BoxPtr box, int e)
{
    if (!BoxElements(box)[e].value)
    {
	RaiseStandardException (exception_uninitialized_value,
				"Uninitialized value", 0);
	return (Void);
    }
    return (BoxElements(box)[e].value);
}
#endif

static void MarkBoxTypes (void *object)
{
    BoxTypesPtr	bt = object;
    int		i;

    for (i = 0; i < bt->count; i++)
	MemReference (BoxTypesValue(bt,i));
}

DataType    BoxTypesType = { MarkBoxTypes, 0 };

#define BT_INCR	4

BoxTypesPtr
NewBoxTypes (int size)
{
    ENTER ();
    BoxTypesPtr    bt;

    bt = ALLOCATE (&BoxTypesType, sizeof (BoxTypes) + size * sizeof (Types *));
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
		    count * sizeof (Types *));
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
