/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

static void MarkFuncCode (void *object)
{
    FuncCodePtr	fc = object;

    MemReference (fc->base.type);
    MemReference (fc->base.args);
    MemReference (fc->code);
    MemReference (fc->body.obj);
    MemReference (fc->body.dynamics);
    MemReference (fc->staticInit.obj);
    MemReference (fc->staticInit.dynamics);
    MemReference (fc->statics);
}

DataType    FuncCodeType = { MarkFuncCode, 0 };

static Bool
HasVarargs (ArgType *args)
{
    while (args)
    {
	if (args->varargs)
	    return True;
	args = args->next;
    }
    return False;
}

CodePtr
NewFuncCode (Types *type, ArgType *args, ExprPtr code)
{
    ENTER ();
    CodePtr	fc;

    fc = ALLOCATE (&FuncCodeType, sizeof (FuncCode));
    fc->base.builtin = False;
    fc->base.type = type;
    fc->base.args = args;
    fc->base.argc = 0;
    fc->base.varargs = HasVarargs (args);
    fc->func.statics = 0;
    fc->func.code = code;
    fc->func.body.dynamics = 0;
    fc->func.body.obj = 0;
    fc->func.staticInit.obj = 0;
    fc->func.staticInit.dynamics = 0;
    RETURN (fc);
}

static void MarkBuiltinCode (void *object)
{
    BuiltinCodePtr	bc = object;

    MemReference (bc->base.type);
    MemReference (bc->base.args);
}

DataType BuiltinCodeType = { MarkBuiltinCode, 0 };

CodePtr
NewBuiltinCode (Types *type, ArgType *args, int argc, 
		BuiltinFunc builtin, Bool needsNext)
{
    ENTER ();
    CodePtr bc;

    bc = ALLOCATE (&BuiltinCodeType, sizeof (BuiltinCode));
    bc->base.builtin = True;
    bc->base.type = type;
    bc->base.args = args;
    bc->base.argc = argc;
    bc->base.varargs = HasVarargs (args);
    bc->builtin.needsNext = needsNext;
    bc->builtin.b = builtin;
    RETURN (bc);
}

static void
FuncMark (void *object)
{
    Func    *f = object;

    MemReference (f->code);
    MemReference (f->staticLink);
    MemReference (f->statics);
}

void printCode (Value f, CodePtr code, int level);

static Bool
FuncPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    PrettyCode (f, av->func.code, 0, class_undef, publish_private, 0, True);
    return True;
}

ValueType   FuncType = {
    { FuncMark, 0 },	/* base */
    {			/* binary */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ValueEqual,
	0,
	0,
    },
    {			    /* unary */
	0,
	0,
	0,
    },
    0,
    0,
    FuncPrint,
    0,
};

Value
NewFunc (CodePtr code, FramePtr staticLink)
{
    ENTER ();
    Value	    ret;

    ret = ALLOCATE (&FuncType.data, sizeof (Func));
    ret->value.tag = type_func;
    ret->func.code = code;
    ret->func.staticLink = staticLink;
    ret->func.statics = 0;
    /*
     * Create the box containing static variables for this closure
     */
    if (!code->base.builtin && code->func.statics)
	ret->func.statics = NewTypedBox (False, False, code->func.statics);
    RETURN (ret);
}
