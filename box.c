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
    Value	*elements;
    int		i;

    elements = BoxElements(box);
    if (box->homogeneous)
	MemReference (box->u.type);
    else
	MemReference (box->u.types);
    for (i = 0; i < box->nvalues; i++)
	MemReference (elements[i]);
}

DataType BoxType = { BoxMark, 0, "BoxType" };

BoxPtr
NewBox (Bool constant, Bool array, int nvalues, TypePtr type)
{
    ENTER ();
    BoxPtr  box;
    int	    i;

    box = ALLOCATE (&BoxType, sizeof (Box) + nvalues * sizeof (Value));
    box->constant = constant;
    box->array = array;
    box->homogeneous = True;
    box->u.type = type;
    box->nvalues = nvalues;
    for (i = 0; i < nvalues; i++)
	BoxValueSet(box, i, 0);
    RETURN (box);
}

BoxPtr
NewTypedBox (Bool array, BoxTypesPtr bt)
{
    ENTER ();
    BoxPtr  box;
    int	    i;

    box = ALLOCATE (&BoxType, sizeof (Box) + bt->count * sizeof (Value));
    box->constant = False;
    box->array = array;
    box->homogeneous = False;
    box->u.types = bt;
    box->nvalues = bt->count;
    for (i = 0; i < bt->count; i++)
	BoxValueSet (box, i, 0);
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

DataType    BoxTypesType = { MarkBoxTypes, 0, "BoxTypesType" };

#define BT_INCR	4

BoxTypesPtr
NewBoxTypes (int size)
{
    ENTER ();
    BoxTypesPtr    bt;

    bt = ALLOCATE (&BoxTypesType, sizeof (BoxTypes) + size * sizeof (Type *));
    bt->size = size;
    bt->count = 0;
    RETURN (bt);
}

int
AddBoxType (BoxTypesPtr *btp, Type *t)
{
    ENTER ();
    BoxTypesPtr bt, new;
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
		    count * sizeof (Type *));
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
