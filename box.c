/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
    if (box->replace)
	MemReference (box->u.replace);
    else if (box->homogeneous)
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
    box->homogeneous = True;
    box->replace = False;
    box->nvalues = nvalues;
    box->u.type = type;
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
    box->homogeneous = False;
    box->replace = False;
    box->nvalues = bt->count;
    box->u.types = bt;
    for (i = 0; i < bt->count; i++)
	BoxValueSet (box, i, 0);
    RETURN (box);
}

static void
MarkBoxReplace (void *object)
{
    BoxReplacePtr   replace = object;

    MemReference (replace->new);
}

DataType    BoxReplaceType = { MarkBoxReplace, 0, "BoxReplaceType" };

void
BoxSetReplace (BoxPtr old, BoxPtr new, int oldstride, int newstride)
{
    ENTER ();
    BoxReplacePtr   r = ALLOCATE (&BoxReplaceType, sizeof (BoxReplace));
    r->new = new;
    r->oldstride = oldstride;
    r->newstride = newstride;
    old->replace = True;
    old->u.replace = r;
    EXIT ();
}

BoxPtr
BoxRewrite (BoxPtr box, int *ep)
{
    int	e = *ep;
    
    while (box->replace)
    {
	BoxReplacePtr	r = box->u.replace;
	int		chunk, off;

	chunk = e / r->oldstride;
	off = e % r->oldstride;
	e = chunk * r->newstride + off;
	box = r->new;
    }
    /*
     * XXX oops.  References to previously available storage
     * should do something sensible instead of cratering.
     * The desired semantic is for them to persist, pointing
     * to whatever storage was there before the underlying object
     * was resized.  But, that's "hard".  This check will
     * at least prevent a seg fault.
     */
    if (e >= box->nvalues)
    {
	RaiseStandardException (exception_invalid_array_bounds, 2,
				Void, NewInt (e));
	e = 0;
	box = NewBox (True, False, 1, typePrim[rep_void]);
	BoxValueSet (box, 0, 0);
    }
    *ep = e;
    return box;
}

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
    BoxTypesValueSet(bt,position,t);
    EXIT ();
    return position;
}
