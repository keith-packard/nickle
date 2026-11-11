/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include "nickle.h"

static void
BoxMark(void *object)
{
    BoxPtr    box = object;
    Value    *elements;
    ptrdiff_t i;

    elements = BoxElements(box);
    if (box->homogeneous)
        MemReference(box->u.type);
    else
        MemReference(box->u.types);
    for (i = 0; i < box->nvalues; i++)
        MemReference(elements[i]);
}

DataType BoxType = { BoxMark, 0, "BoxType" };

BoxPtr
NewBox(Bool constant, Bool array, ptrdiff_t nvalues, TypePtr type)
{
    ENTER();
    BoxPtr    box;
    ptrdiff_t i;

    (void)array;
    box = ALLOCATE(&BoxType, sizeof(Box) + (size_t)nvalues * sizeof(Value));
    box->constant = constant;
    box->homogeneous = True;
    assert(0 <= nvalues && nvalues <= BOX_MAX_VALUES);
    box->nvalues = nvalues & BOX_MAX_VALUES;
    box->u.type = type;
    for (i = 0; i < nvalues; i++)
        BoxValueSet(box, i, 0);
    RETURN(box);
}

BoxPtr
NewTypedBox(Bool array, BoxTypesPtr bt)
{
    ENTER();
    BoxPtr    box;
    ptrdiff_t i;

    (void)array;
    assert(0 <= bt->count && bt->count <= BOX_MAX_VALUES);
    box = ALLOCATE(&BoxType, sizeof(Box) + (size_t)bt->count * sizeof(Value));
    box->constant = False;
    box->homogeneous = False;
    box->nvalues = bt->count & BOX_MAX_VALUES;
    box->u.types = bt;
    for (i = 0; i < bt->count; i++)
        BoxValueSet(box, i, 0);
    RETURN(box);
}

static void
MarkBoxTypes(void *object)
{
    BoxTypesPtr bt = object;
    ptrdiff_t   i;

    for (i = 0; i < bt->count; i++)
        MemReference(BoxTypesValue(bt, i));
}

static DataType BoxTypesType = { MarkBoxTypes, 0, "BoxTypesType" };

#define BT_INCR 4

BoxTypesPtr
NewBoxTypes(ptrdiff_t size)
{
    ENTER();
    BoxTypesPtr bt;

    bt = ALLOCATE(&BoxTypesType, sizeof(BoxTypes) + (size_t)size * sizeof(Type *));
    bt->size = size;
    bt->count = 0;
    RETURN(bt);
}

ptrdiff_t
AddBoxType(BoxTypesPtr *btp, Type *t)
{
    ENTER();
    BoxTypesPtr bt, new;
    ptrdiff_t   count, size;
    ptrdiff_t   position;

    bt = *btp;
    if (!bt) {
        count = 0;
        size = 0;
    } else {
        count = bt->count;
        size = bt->size;
    }
    if (count == size) {
        /* Double the size, so that adding n types copies O(n) of them */
        size = size ? size * 2 : BT_INCR;
        new = NewBoxTypes(size);
        if (count) {
            memcpy(BoxTypesElements(new), BoxTypesElements(bt), (size_t)count * sizeof(Type *));
        }
        new->size = size;
        new->count = count;
        *btp = new;
        bt = new;
    }
    position = bt->count++;
    BoxTypesValueSet(bt, position, t);
    EXIT();
    return position;
}
