/*
 * Copyright © 2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

static Value
ForeignEqual (Value av, Value bv, int expandOk)
{
    if (av->foreign.data == bv->foreign.data)
	return TrueVal;
    return FalseVal;
}

static Bool
ForeignPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    FilePrintf (f, "foreign %s (0x%x)", av->foreign.id, av->foreign.data);
    return True;
}

static HashValue
ForeignHash (Value av)
{
    return (HashValue) (intptr_t) av->foreign.data;
}

static void
ForeignMark (void *object)
{
    Foreign *foreign = object;

    if (foreign->mark)
	(*foreign->mark) (foreign->data);
}

static int
ForeignFree (void *object)
{
    Foreign *foreign = object;

    if (foreign->free)
	(*foreign->free) (foreign->data);
    return 1;
}

ValueRep ForeignRep = {
    { ForeignMark, ForeignFree, "ForeignRep" },	    /* data */
    rep_foreign,	    /* tag */
    {		    /* binary */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ForeignEqual,
	0,
	0
    },
    {
	0,
	0,
	0,
    },
    0, 0,
    ForeignPrint,
    0,
    ForeignHash,
};

int
ForeignInit (void)
{
    return 1;
}

Value
NewForeign (const char *id, void *data, void (*mark) (void *data), void (*free) (void *data))
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&ForeignRep.data, sizeof (Foreign));
    ret->foreign.id = id;
    ret->foreign.data = data;
    ret->foreign.free = free;
    ret->foreign.mark = mark;
    RETURN (ret);
}
