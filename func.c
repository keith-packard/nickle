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

static void MarkFuncCode (void *object)
{
    FuncCodePtr	fc = object;

    MemReference (fc->locals);
    MemReference (fc->args);
    MemReference (fc->code);
    MemReference (fc->obj);
    MemReference (fc->staticInit);
}

DataType    FuncCodeType = { MarkFuncCode, 0 };

CodePtr
NewFuncCode (Type type, ExprPtr args, ExprPtr code)
{
    ENTER ();
    CodePtr	fc;

    fc = ALLOCATE (&FuncCodeType, sizeof (FuncCode));
    fc->base.builtin = False;
    fc->base.type = type;
    fc->func.autoc = 0;
    fc->func.locals = 0;
    fc->func.args = args;
    fc->func.code = code;
    fc->func.obj = 0;
    fc->func.staticc = 0;
    fc->func.staticInit = 0;
    RETURN (fc);
}

static void MarkBuiltinCode (void *object)
{
}

DataType BuiltinCodeType = { MarkBuiltinCode, 0 };

CodePtr
NewBuiltinCode (Type type, int argc, BuiltinFunc builtin)
{
    ENTER ();
    CodePtr  bc;

    bc = ALLOCATE (&BuiltinCodeType, sizeof (BuiltinCode));
    bc->base.builtin = True;
    bc->base.type = type;
    bc->base.argc = argc;
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
    MemReference (f->staticInit);
}

void printCode (Value f, CodePtr code, int level);

Bool
FuncPrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    PrintCode (f, av->func.code, 0, publish_private, 0, True);
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
	0,
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
    ScopeChainPtr   chain;
    SymbolPtr	    statics;

    ret = ALLOCATE (&FuncType.data, sizeof (Func));
    ret->value.tag = type_func;
    ret->func.code = code;
    ret->func.staticLink = staticLink;
    ret->func.statics = 0;
    /*
     * Create the box containing static variables for this closure
     */
    if (!code->base.builtin)
    {
	if (code->func.staticc)
	{
	    ret->func.statics = NewBox (False, code->func.staticc);
	    for (chain = code->func.locals->symbols;
		 chain;
		 chain = chain->next)
	    {
		statics = chain->symbol;
		if (statics->symbol.class == class_static)
		    BoxValue (ret->func.statics, 
			      statics->local.element) = Default (statics->symbol.type);
	    }
	}
    }
    RETURN (ret);
}
