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

Value
RefPlus (Value av, Value bv, int expandOk)
{
    ENTER();
    int	    i;
    Ref	    *ref;

    if (av->value.tag == type_int)
    {
	i = IntPart (av, "Attempt to add non-integer to reference type");
	if (aborting)
	    RETURN (Zero);
	ref = &bv->ref;
    }
    else if (bv->value.tag == type_int)
    {
	i = IntPart (bv, "Attempt to add non-integer to reference type");
	if (aborting)
	    RETURN (Zero);
	ref = &av->ref;
    }
    else
	RETURN (Zero);
    i = i + ref->element;
    if (i < 0 || i >= ref->box->nvalues)
    {
	RaiseError ("Element out of range in reference addition %v + %v", av, bv);
	RETURN (Zero);
    }
    RETURN (NewRef (ref->box, i));
}

Value
RefMinus (Value av, Value bv, int expandOk)
{
    ENTER();
    int	    i;
    int	    element;
    Ref	    *ref, *bref;

    if (av->value.tag == type_int)
    {
	i = IntPart (av, "Attempt to subtract non-integer to reference type");
	if (aborting)
	    RETURN (Zero);
	ref = &bv->ref;
	element = -ref->element;
    }
    else if (bv->value.tag == type_int)
    {
	i = -IntPart (bv, "Attempt to subtract non-integer to reference type");
	if (aborting)
	    RETURN (Zero);
	ref = &av->ref;
	element = ref->element;
    }
    else
    {
	ref = &av->ref;
	bref = &bv->ref;
	if (ref->box != bref->box)
	{
	    RaiseError ("Attempt to subtract references to different objects %v - %v", av, bv);
	    RETURN (Zero);
	}
	RETURN (NewInt (ref->element - bref->element));
    }
    i = i + element;
    if (i < 0 || i >= ref->box->nvalues)
    {
	RaiseError ("Element out of range in reference subtraction %v - %v", av, bv);
	RETURN (Zero);
    }
    RETURN (NewRef (ref->box, i));
}

ValueType *
RefTypeCheck (BinaryOp op, Value av, Value bv, int expandOk)
{
    switch (op) {
    case MinusOp:
	if (av->value.tag == type_ref && bv->value.tag == type_ref)
	    return av->value.type;
    case PlusOp:
	if (av->value.tag == type_int)
	    return bv->value.type;
	if (bv->value.tag == type_int)
	    return av->value.type;
	break;
    default:
	break;
    }
    return 0;
}

Bool
RefPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FileOutput (f, '&');
    return Print (f, RefValue (av), format, base, width ? width - 1 : 0, prec, fill);
}

    
static void
RefMark (void *object)
{
    Ref	*ref = object;

    MemReference (ref->box);
}

ValueType RefType = { 
    { RefMark, 0 },	/* data */
    {			/* binary */
	RefPlus,
	RefMinus,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
    },
    {			/* unary */
	0,
	0,
	0,
    },
    0,
    0,
    RefPrint,
    RefTypeCheck,
};
    
Value
NewRef (BoxPtr box, int element)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&RefType.data, sizeof (Ref));
    ret->value.tag = type_ref;
    ret->ref.box = box;
    ret->ref.element = element;
    RETURN (ret);
}
