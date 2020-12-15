/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"

static void MarkFuncCode (void *object)
{
    FuncCodePtr	fc = object;

    MemReference (fc->base.type);
    MemReference (fc->base.args);
    MemReference (fc->base.name);
    MemReference (fc->base.previous);
    MemReference (fc->base.func);
    MemReference (fc->base.doc);
    MemReference (fc->code);
    MemReference (fc->body.obj);
    MemReference (fc->body.dynamics);
    MemReference (fc->staticInit.obj);
    MemReference (fc->staticInit.dynamics);
    MemReference (fc->statics);
}

DataType    FuncCodeType = { MarkFuncCode, 0, "FuncCodeType" };

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
NewFuncCode (Type *type, ArgType *args, ExprPtr code, Value doc)
{
    ENTER ();
    CodePtr	fc;

    fc = ALLOCATE (&FuncCodeType, sizeof (FuncCode));
    fc->base.builtin = False;
    fc->base.type = type;
    fc->base.argc = 0;
    fc->base.varargs = HasVarargs (args);
    fc->base.args = args;
    fc->base.name = 0;
    fc->base.previous = 0;
    fc->base.func = fc;
    fc->base.doc = doc;
    
    fc->func.code = code;
    
    fc->func.body.dynamics = 0;
    fc->func.body.obj = 0;
    fc->func.staticInit.obj = 0;
    fc->func.staticInit.dynamics = 0;
    
    fc->func.statics = 0;
    fc->func.inStaticInit = False;
    fc->func.inGlobalInit = False;
    RETURN (fc);
}

static void MarkBuiltinCode (void *object)
{
    BuiltinCodePtr	bc = object;

    MemReference (bc->base.type);
    MemReference (bc->base.args);
    MemReference (bc->base.name);
    MemReference (bc->base.previous);
    MemReference (bc->base.func);
    MemReference (bc->base.doc);
}

DataType BuiltinCodeType = { MarkBuiltinCode, 0, "BuiltinCodeType" };

CodePtr
NewBuiltinCode (Type *type, ArgType *args, int argc, 
		BuiltinFunc builtin, Bool needsNext, char *doc)
{
    ENTER ();
    CodePtr bc;

    bc = ALLOCATE (&BuiltinCodeType, sizeof (BuiltinCode));
    bc->base.builtin = True;
    bc->base.type = type;
    bc->base.argc = argc;
    bc->base.varargs = HasVarargs (args);
    bc->base.args = args;
    bc->base.name = 0;
    bc->base.previous = 0;
    bc->base.func = 0;
    bc->base.doc = doc ? NewStrString (doc) : Void;
    
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
FuncPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    Bool    nest = False;

    if (format == 'v')
	nest = True;
    PrettyCode (f, av->func.code, 0, class_undef, publish_private, 0, nest);
    return True;
}

ValueRep   FuncRep = {
    { FuncMark, 0, "FuncRep" },	/* base */
    rep_func,		/* tag */
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

    ret = ALLOCATE (&FuncRep.data, sizeof (Func));
    ret->func.code = code;
    ret->func.staticLink = staticLink;
    ret->func.statics = 0;
    /*
     * Create the box containing static variables for this closure
     */
    if (!code->base.builtin && code->func.statics)
	ret->func.statics = NewTypedBox (False, code->func.statics);
    RETURN (ret);
}
