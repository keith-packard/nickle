/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

static void
FrameMark (void *object)
{
    Frame   *frame = object;

    do
    {
	MemReference (frame->staticLink);
	MemReference (frame->function);
	MemReference (frame->frame);
	MemReference (frame->statics);
	MemReference (frame->saveObj);
	frame = frame->previous;
    } while (MemReferenceNoRecurse (frame) == 0);
}

DataType FrameType = { FrameMark, 0, "FrameType" };

FramePtr
NewFrame (Value		function,
	  FramePtr	previous,
	  FramePtr	staticLink,
	  BoxTypesPtr	dynamics,
	  BoxPtr	statics)
{
    ENTER ();
    FramePtr	frame;

    frame = ALLOCATE (&FrameType, sizeof (Frame));
    frame->previous = previous;
    frame->staticLink = staticLink;
    frame->function = function;
    frame->savePc = 0;
    frame->saveObj = 0;
    frame->statics = statics;
    frame->frame = 0;
    if (dynamics)
	frame->frame = NewTypedBox (False, dynamics);
    RETURN (frame);
}
