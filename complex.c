/*
 * Copyright © 2025 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<math.h>
#include	"nickle.h"

static Value
ComplexPlus (Value av, Value bv, int expandOk)
{
    (void) expandOk;
    ENTER();
    Value ret = NewValueComplex(Plus(av->complex.r, bv->complex.r),
				Plus(av->complex.i, bv->complex.i));
    RETURN(ret);
}

static Value
ComplexMinus (Value av, Value bv, int expandOk)
{
    (void) expandOk;
    ENTER();
    Value ret = NewValueComplex(Minus(av->complex.r, bv->complex.r),
				Minus(av->complex.i, bv->complex.i));
    RETURN(ret);
}

static Value
ComplexTimes (Value av, Value bv, int expandOk)
{
    (void) expandOk;
    ENTER();
    Value ret = NewValueComplex(Minus(Times(av->complex.r, bv->complex.r),
				      Times(av->complex.i, bv->complex.i)),
				Plus(Times(av->complex.r, bv->complex.i),
				     Times(av->complex.i, bv->complex.r)));
    RETURN(ret);
}

static Value
ComplexRecip (Value av)
{
    ENTER();
    Value modsq = Plus(Times(av->complex.r, av->complex.r),
		       Times(av->complex.i, av->complex.i));
    Value ret = NewValueComplex(Divide(av->complex.r, modsq),
				Divide(Negate(av->complex.i), modsq));
    RETURN(ret);
}

static Value
ComplexDivide (Value av, Value bv, int expandOk)
{
    (void) expandOk;
    ENTER();
    Value recip = ComplexRecip(bv);
    Value ret = ComplexTimes(av, recip, expandOk);
    RETURN(ret);
}

static Value
ComplexLess (Value av, Value bv, int expandOk)
{
    (void) expandOk;
    /* Dictionary ordering is at least consistent */
    if (True(Less(av->complex.r, bv->complex.r)))
	return TrueVal;
    if (True(Equal(av->complex.r, bv->complex.r)))
	return Less(av->complex.i, bv->complex.i);
    return FalseVal;
}

static Value
ComplexEqual (Value av, Value bv, int expandOk)
{
    (void) expandOk;
    Value	ret;
    ret = Equal(av->complex.r, bv->complex.r);
    if (True(ret))
	ret = Equal(av->complex.i, bv->complex.i);
    return ret;
}

static Value
ComplexNegate (Value av, int expandOk)
{
    (void) expandOk;
    ENTER();
    Value ret = NewValueComplex(Negate(av->complex.r), Negate(av->complex.i));
    RETURN(ret);
}

static Value
ComplexFloor (Value av, int expandOk)
{
    (void) expandOk;
    ENTER();
    Value ret = NewValueComplex(Floor(av->complex.r), Floor(av->complex.i));
    RETURN(ret);
}

static Value
ComplexCeil (Value av, int expandOk)
{
    (void) expandOk;
    ENTER();
    Value ret = NewValueComplex(Ceil(av->complex.r), Ceil(av->complex.i));
    RETURN(ret);
}

static Value
ComplexPromote (Value av, Value bv)
{
    (void) bv;
    ENTER ();
    if (!ValueIsComplex(av))
	av = NewComplex(av, Zero);
    RETURN(av);
}

static Value
ComplexReduce (Value av)
{
    if (Zerop(av->complex.i))
	return av->complex.r;
    return av;
}

static Bool
ComplexPrint(Value f, Value fv, char format, int base, int width, int prec, int fill)
{
    Bool	    value = format == 'v';

    if (value)
	FilePuts(f, "cmplx(");
    if (!Print(f, fv->complex.r, format, base, width, prec, fill))
	return False;
    if (value)
	FilePuts(f, ", ");
    else
	FilePuts(f, "+i");
    if (!Print(f, fv->complex.i, format, base, width, prec, fill))
	return False;
    if (value)
	FilePuts(f, ")");
    return True;
}

static HashValue
ComplexHash (Value av)
{
    return ValueInt(ValueHash(av->complex.r)) ^ ValueInt(ValueHash(av->complex.i));
}

static void
ComplexMark (void *object)
{
    Complex	*c = object;

    MemReference(c->r);
    MemReference(c->i);
}

ValueRep   ComplexRep = {
    { ComplexMark, 0, "ComplexRep" },	/* base */
    rep_complex,	/* tag */
    {			/* binary */
	ComplexPlus,
	ComplexMinus,
	ComplexTimes,
	ComplexDivide,
	NumericDiv,
	NumericMod,
	ComplexLess,
	ComplexEqual,
	0,
	0,
    },
    {			/* unary */
	ComplexNegate,
	ComplexFloor,
	ComplexCeil,
    },
    ComplexPromote,
    ComplexReduce,
    ComplexPrint,
    0,
    ComplexHash,
};

Value
NewComplex (Value r, Value i)
{
    ENTER();
    Value	ret;

    ret = ALLOCATE (&ComplexRep.data, sizeof (Complex));
    ret->complex.r = r;
    ret->complex.i = i;
    RETURN (ret);
}

Value
NewValueComplex (Value r, Value i)
{
    if (!Zerop(i))
	r = NewComplex(r, i);
    return r;
}
