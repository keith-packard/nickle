/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<config.h>

#include	"nickle.h"

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
NewSymbolGlobal (Atom name, Type type, Publish publish)
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
NewSymbolArg (Atom name, Type type)
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
NewSymbolAuto (Atom name, Type type)
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
NewSymbolStatic (Atom name, Type type)
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
NewSymbolStruct (Atom name, StructType *type, Publish publish)
{
    ENTER ();
    SymbolPtr	s;

    s = ALLOCATE (&SymbolStructType, sizeof (SymbolStruct));
    s->symbol.name = name;
    s->symbol.class = class_struct;
    s->symbol.type = type_struct;
    s->symbol.publish = publish;
    s->structs.type = type;
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
    s->symbol.type = type_undef;
    s->symbol.publish = publish;
    s->scope.scope = scope;
    RETURN (s);
}

void
SymbolInit ()
{
}
