/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

static void
SymbolTypeMark (void *object)
{
    SymbolType	    *st = object;

    MemReference (st->symbol.next);
    MemReference (st->symbol.type);
}

static void
SymbolGlobalMark (void *object)
{
    SymbolGlobal    *sg = object;

    MemReference (sg->symbol.next);
    MemReference (sg->symbol.type);
    MemReference (sg->value);
}

static void
SymbolLocalMark (void *object)
{
    SymbolLocal	    *sl = object;

    MemReference (sl->symbol.next);
    MemReference (sl->symbol.type);
}

static void
SymbolNamespaceMark (void *object)
{
    SymbolNamespace *sn = object;

    MemReference (sn->symbol.next);
    MemReference (sn->symbol.type);
    MemReference (sn->namespace);
}

DataType    SymbolTypeType = { SymbolTypeMark, 0, "SymbolTypeType" };
DataType    SymbolGlobalType = { SymbolGlobalMark, 0, "SymbolGlobalType" };
DataType    SymbolLocalType = { SymbolLocalMark, 0, "SymbolLocalType" };
DataType    SymbolNamespaceType = { SymbolNamespaceMark, 0, "SymbolNamespaceType" };

SymbolPtr
NewSymbolType (Atom name, Type *type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolTypeType, sizeof (SymbolType));
    s->symbol.next = 0;
    s->symbol.name = name;
    s->symbol.class = class_typedef;
    s->symbol.type = type;
    s->symbol.forward = False;
    RETURN (s);
}

SymbolPtr
NewSymbolException (Atom name, Type *type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolTypeType, sizeof (SymbolType));
    s->symbol.next = 0;
    s->symbol.name = name;
    s->symbol.class = class_exception;
    s->symbol.type = type;
    s->symbol.forward = False;
    RETURN (s);
}

SymbolPtr
NewSymbolConst (Atom name, Type *type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolGlobalType, sizeof (SymbolGlobal));
    s->symbol.next = 0;
    s->symbol.name = name;
    s->symbol.class = class_const;
    s->symbol.type = type;
    s->symbol.forward = False;
    s->global.value = NewBox (True, False, 1, type);
    RETURN (s);
}

SymbolPtr
NewSymbolGlobal (Atom name, Type *type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolGlobalType, sizeof (SymbolGlobal));
    s->symbol.next = 0;
    s->symbol.name = name;
    s->symbol.class = class_global;
    s->symbol.type = type;
    s->symbol.forward = False;
    s->global.value = NewBox (False, False, 1, type);
    RETURN (s);
}

SymbolPtr
NewSymbolArg (Atom name, Type *type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolLocalType, sizeof (SymbolLocal));
    s->symbol.next = 0;
    s->symbol.name = name;
    s->symbol.class = class_arg;
    s->symbol.type = type;
    s->symbol.forward = False;
    s->local.element = 0;
    RETURN (s);
}

SymbolPtr
NewSymbolAuto (Atom name, Type *type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolLocalType, sizeof (SymbolLocal));
    s->symbol.next = 0;
    s->symbol.name = name;
    s->symbol.class = class_auto;
    s->symbol.type = type;
    s->symbol.forward = False;
    s->local.element = -1;
    RETURN (s);
}

SymbolPtr
NewSymbolStatic (Atom name, Type *type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolLocalType, sizeof (SymbolLocal));
    s->symbol.next = 0;
    s->symbol.name = name;
    s->symbol.class = class_static;
    s->symbol.type = type;
    s->symbol.forward = False;
    s->local.element = -1;
    RETURN (s);
}

SymbolPtr
NewSymbolNamespace (Atom name, NamespacePtr namespace)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolNamespaceType, sizeof (SymbolNamespace));
    s->symbol.next = 0;
    s->symbol.name = name;
    s->symbol.class = class_namespace;
    s->symbol.type = 0;
    s->symbol.forward = False;
    s->namespace.namespace = namespace;
    RETURN (s);
}

void
SymbolInit ()
{
}
