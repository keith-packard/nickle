/*
 * $Id$
 *
 * Copyright 1996 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include    "nick.h"

void
FrameMark (void *object)
{
    Frame   *frame = object;

    MemReference (frame->previous);
    MemReference (frame->staticLink);
    MemReference (frame->function);
    MemReference (frame->frame);
    MemReference (frame->saveCode);
}

DataType FrameType = { FrameMark, 0 };

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
    frame->frame = NewTypedBox (False, dynamics);
    frame->statics = statics;
    RETURN (frame);
}
