/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"mem.h"
#include	"ref.h"

typedef struct _reference {
    DataType	*data;
    void	**object;
} Reference;

static void
ReferenceMark (void *object)
{
    ReferencePtr    reference = object;

    MemReference (*reference->object);
}

static DataType referenceType = { ReferenceMark, 0 };

ReferencePtr
NewReference (void **object)
{
    ReferencePtr    reference;

    reference = MemAllocate (&referenceType, sizeof (Reference));
    reference->object = object;
    return reference;
}
