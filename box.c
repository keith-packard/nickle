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

#include "value.h"

static void
BoxMark (void *object)
{
    BoxPtr	box = object;
    BoxElement	*elements;
    int		i;

    elements = BoxElements(box);
    for (i = 0; i < box->nvalues; i++)
	MemReference (elements[i].value);
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
	BoxType(box, i) = type_undef;
	BoxValue(box, i) = Zero;
    }	
    RETURN (box);
}

#ifdef DEBUG
void
BoxDump (BoxPtr box)
{
    int	    i;

    for (i = 0; i < box->nvalues; i++)
	RaiseError ("%d: (%T) %v\n", i, BoxType(box, i), BoxValue (box, i));
}
#endif
