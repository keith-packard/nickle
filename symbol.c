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

# include "nick.h"

#if 0
static void
SymbolTypeMark (void *object)
{
    SymbolType	    *st = object;

}
#endif

static void
SymbolGlobalMark (void *object)
{
    SymbolGlobal    *sg = object;

    MemReference (sg->symbol.next);
    MemReference (sg->value);
}

static void
SymbolLocalMark (void *object)
{
    SymbolLocal	    *sl = object;

    MemReference (sl->symbol.next);
}

static void
SymbolStructMark (void *object)
{
    SymbolStruct    *ss = object;

    MemReference (ss->symbol.next);
    MemReference (ss->type);
}

static void
SymbolScopeMark (void *object)
{
    SymbolScope	    *ss = object;

    MemReference (ss->symbol.next);
    MemReference (ss->scope);
}

#if 0
DataType    SymbolTypeType = { SymbolTypeMark, 0 };
#endif
DataType    SymbolGlobalType = { SymbolGlobalMark, 0 };
DataType    SymbolLocalType = { SymbolLocalMark, 0 };
DataType    SymbolStructType = { SymbolStructMark, 0 };
DataType    SymbolScopeType = { SymbolScopeMark, 0 };

SymbolPtr
NewSymbolGlobal (Atom name, Type type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolGlobalType, sizeof (SymbolGlobal));
    s->symbol.name = name;
    s->symbol.class = class_global;
    s->symbol.type = type;
    s->global.value = NewBox (False, 1);
    BoxType (s->global.value, 0) = type;
    BoxValue (s->global.value, 0) = Default (type);
    RETURN (s);
}

SymbolPtr
NewSymbolArg (Atom name, Type type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolLocalType, sizeof (SymbolLocal));
    s->symbol.name = name;
    s->symbol.class = class_arg;
    s->symbol.type = type;
    s->local.element = 0;
    RETURN (s);
}

SymbolPtr
NewSymbolAuto (Atom name, Type type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolLocalType, sizeof (SymbolLocal));
    s->symbol.name = name;
    s->symbol.class = class_auto;
    s->symbol.type = type;
    s->local.element = -1;
    RETURN (s);
}

SymbolPtr
NewSymbolStatic (Atom name, Type type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolLocalType, sizeof (SymbolLocal));
    s->symbol.name = name;
    s->symbol.class = class_static;
    s->symbol.type = type;
    s->local.element = -1;
    RETURN (s);
}

SymbolPtr
NewSymbolStruct (Atom name, StructType *type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolStructType, sizeof (SymbolStruct));
    s->symbol.name = name;
    s->symbol.class = class_struct;
    s->symbol.type = type_struct;
    s->structs.type = type;
    RETURN (s);
}

SymbolPtr
NewSymbolScope (Atom name, ScopePtr scope)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolScopeType, sizeof (SymbolScope));
    s->symbol.name = name;
    s->symbol.class = class_scope;
    s->symbol.type = type_undef;
    s->scope.scope = scope;
    RETURN (s);
}

void
SymbolInit ()
{
}
