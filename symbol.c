/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

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
SymbolScopeMark (void *object)
{
    SymbolScope	    *ss = object;

    MemReference (ss->symbol.next);
    MemReference (ss->symbol.type);
    MemReference (ss->scope);
}

DataType    SymbolTypeType = { SymbolTypeMark, 0 };
DataType    SymbolGlobalType = { SymbolGlobalMark, 0 };
DataType    SymbolLocalType = { SymbolLocalMark, 0 };
DataType    SymbolScopeType = { SymbolScopeMark, 0 };

SymbolPtr
NewSymbolType (Atom name, Types *type, Publish publish)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolTypeType, sizeof (SymbolType));
    s->symbol.name = name;
    s->symbol.class = class_typedef;
    s->symbol.type = type;
    s->symbol.publish = publish;
    RETURN (s);
}

SymbolPtr
NewSymbolGlobal (Atom name, Types *type, Publish publish)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolGlobalType, sizeof (SymbolGlobal));
    s->symbol.name = name;
    s->symbol.class = class_global;
    s->symbol.type = type;
    s->symbol.publish = publish;
    s->global.value = NewBox (False, 1);
    BoxType (s->global.value, 0) = type;
    BoxValue (s->global.value, 0) = Default (type);
    RETURN (s);
}

SymbolPtr
NewSymbolArg (Atom name, Types *type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolLocalType, sizeof (SymbolLocal));
    s->symbol.name = name;
    s->symbol.class = class_arg;
    s->symbol.type = type;
    s->symbol.publish = publish_private;
    s->local.element = 0;
    RETURN (s);
}

SymbolPtr
NewSymbolAuto (Atom name, Types *type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolLocalType, sizeof (SymbolLocal));
    s->symbol.name = name;
    s->symbol.class = class_auto;
    s->symbol.type = type;
    s->symbol.publish = publish_private;
    s->local.element = -1;
    RETURN (s);
}

SymbolPtr
NewSymbolStatic (Atom name, Types *type)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolLocalType, sizeof (SymbolLocal));
    s->symbol.name = name;
    s->symbol.class = class_static;
    s->symbol.type = type;
    s->symbol.publish = publish_private;
    s->local.element = -1;
    RETURN (s);
}

SymbolPtr
NewSymbolScope (Atom name, ScopePtr scope, Publish publish)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolScopeType, sizeof (SymbolScope));
    s->symbol.name = name;
    s->symbol.class = class_scope;
    s->symbol.type = 0;
    s->symbol.publish = publish;
    s->scope.scope = scope;
    RETURN (s);
}

void
SymbolInit ()
{
}
