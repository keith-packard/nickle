/* $Header$ */
/*
 * This program is Copyright (C) 1988 by Keith Packard.  IC is provided to
 * you without charge, and with no warranty.  You may give away copies of
 * IC, including source, provided that this notice is included in all the
 * files.
 */
/*
 * operators accepting values
 */

#include "value.h"

extern ValueType    IntType, IntegerType, RationalType, DoubleType;
extern ValueType    StringType, FileType;

volatile Bool	aborting;
volatile Bool	exception;

ValueType   *valueTypes[] = {
    &IntType, &IntegerType, &RationalType, &DoubleType,
    &StringType, &FileType
};

Bool
Numericp (Type t)
{
    switch (t) {
    case type_int:
    case type_integer:
    case type_rational:
    case type_double:
	return True;
    default:;
    }
    return False;
}

Bool
Integralp (Type t)
{
    switch (t) {
    case type_int:
    case type_integer:
	return True;
    default:;
    }
    return False;
}

Bool
Zerop (Value av)
{
    switch (av->value.tag) {
    case type_int:
	return av->ints.value == 0;
    case type_integer:
	return av->integer.mag->length == 0;
    case type_rational:
	return av->rational.num->length == 0;
    case type_double:
	return av->doubles.value == 0;
    default:;
    }
    return False;
}

Bool
Negativep (Value av)
{
    switch (av->value.tag) {
    case type_int:
	return av->ints.value < 0;
    case type_integer:
	return av->integer.sign == Negative;
    case type_rational:
	return av->rational.sign == Negative;
    case type_double:
	return av->doubles.value < 0.0;
    default:;
    }
    return False;
}

Bool
Evenp (Value av)
{
    switch (av->value.tag) {
    case type_int:
	return (av->ints.value & 1) == 0;
    case type_integer:
	return NaturalEven (av->integer.mag);
    default:;
    }
    return False;
}

/*
 * Check an assignment for type compatibility; Lvalues can assert
 * maximal domain for their values
 */

Bool
AssignTypeCompatiblep (Type dest, Value v)
{
    if (dest != type_undef && dest != v->value.tag)
    {
	if (!Numericp (dest))
	{
	    switch (dest) {
	    case type_string:
	    case type_array:
	    case type_struct:
		break;
	    default:
		if (Zerop (v))
		    return True;
		break;
	    }
	    return False;
	}
	if (dest < v->value.tag)
	    return False;
    }
    return True;
}

int
IntPart (Value av, char *error)
{
    av = Truncate (av);
    if (av->value.tag != type_int)
    {
	RaiseError ("%s %v", error, av);
	return 0;
    }
    return av->ints.value;
}

double
DoublePart (Value av)
{
    if (!Numericp (av->value.tag))
	return 0.0;
    return DoubleType.promote (av)->doubles.value;
}

Value
BinaryOperate (Value av, Value bv, BinaryOp operator)
{
    ENTER ();
    Value	ret;
    ValueType	*type = 0;

    if (av->value.type->typecheck)
	type = (*av->value.type->typecheck) (operator, av, bv, 1);
    else if (bv->value.type->typecheck)
	type = (*bv->value.type->typecheck) (operator, av, bv, 1);
    else if (av->value.tag == bv->value.tag)
	type = av->value.type;
    else if (Numericp (av->value.tag) && Numericp (bv->value.tag))
    {
	if (av->value.tag < bv->value.tag)
	    av = (*bv->value.type->promote) (av);
	else
	    bv = (*av->value.type->promote) (bv);
	type = av->value.type;
    }
    if (!type || !type->binary[operator])
    {
	if (operator != EqualOp)
	{
	    RaiseError ("undefined type combination %T %O %T",
		    av->value.tag, operator, bv->value.tag);
	}
	RETURN (Zero);
    }
    ret = (*type->binary[operator]) (av, bv, 1);
    if (ret->value.type->reduce)
	ret = (*ret->value.type->reduce) (ret);
    RETURN (ret);
}

Value
UnaryOperate (Value v, UnaryOp operator)
{
    ENTER ();
    Value   ret;
    
    if (!v->value.type->unary[operator])
    {
	RaiseError ("undefined operator %T %U", v->value.tag, operator);
	RETURN (Zero);
    }
    ret = (*v->value.type->unary[operator])(v, 1);
    if (ret->value.type->reduce)
	ret = (*ret->value.type->reduce) (ret);
    RETURN (ret);
}

Value
Reduce (Value v)
{
    ENTER ();
    if (v->value.type->reduce)
	v = (*v->value.type->reduce) (v);
    RETURN (v);
}

Value
Plus (Value av, Value bv)
{
    return BinaryOperate (av, bv, PlusOp);
}

Value
Minus (Value av, Value bv)
{
    return BinaryOperate (av, bv, MinusOp);
}

Value
Times (Value av, Value bv)
{
    return BinaryOperate (av, bv, TimesOp);
}

Value
Divide (Value av, Value bv)
{
    return BinaryOperate (av, bv, DivideOp);
}

Value
Div (Value av, Value bv)
{
    return BinaryOperate (av, bv, DivOp);
}

Value
Mod (Value av, Value bv)
{
    return BinaryOperate (av, bv, ModOp);
}

Value
NumericDiv (Value av, Value bv, int expandOk)
{
    ENTER ();
    av = Minus (av, Mod (av, bv));
    RETURN (Divide (av, bv));
}

Value
Less (Value av, Value bv)
{
    return BinaryOperate (av, bv, LessOp);
}

Value
Equal (Value av, Value bv)
{
    return BinaryOperate (av, bv, EqualOp);
}

Value
Land (Value av, Value bv)
{
    return BinaryOperate (av, bv, LandOp);
}

Value
Lor (Value av, Value bv)
{
    return BinaryOperate (av, bv, LorOp);
}

Value
Negate (Value av)
{
    return UnaryOperate (av, NegateOp);
}

Value
Floor (Value av)
{
    return UnaryOperate (av, FloorOp);
}

Value
Ceil (Value av)
{
    return UnaryOperate (av, CeilOp);
}

/*
 * non primitive functions
 */

Value
Lnot (Value av)
{
    ENTER ();
    RETURN (Minus (Negate (av), One));
}

Value
Lxor (Value av, Value bv)
{
    ENTER ();
    RETURN (Land (Lnot (Land (av, bv)),
		  Lor (av, bv))); 
}

Value
Not (Value av)
{
    ENTER ();

    if (Zerop (av))
	RETURN (One);
    else
	RETURN (Zero);
}

Value
Greater (Value av, Value bv)
{
    return Less (bv, av);
}

Value
LessEqual (Value av, Value bv)
{
    return Not (Less (bv, av));
}

Value
GreaterEqual (Value av, Value bv)
{
    return Not (Less (av, bv));
}

Value
NotEqual (Value av, Value bv)
{
    return Not (Equal (av, bv));
}

Value
Factorial (Value av)
{
    ENTER ();
    Value   tv;

    if (exception || !Zerop (Less (av, One))) 
	RETURN (One);
    tv = Factorial (Minus (av, One));
    if (!exception)
	tv = Times (av, tv);
    RETURN (tv);
}

Value
Truncate (Value av)
{
    ENTER ();
    if (Negativep (av))
	av = Ceil (av);
    else
	av = Floor (av);
    RETURN (av);
}

Value
Pow (Value av, Value bv)
{
    ENTER ();
    Value	result;

    if (!Numericp (av->value.tag) || !Numericp (bv->value.tag))
    {
	RaiseError ("undefined type combination %T ^ %T",
		    av->value.tag, bv->value.tag);
	RETURN (Zero);
    }
    switch (bv->value.tag) {
    case type_int:
	{
	    Value	p;
	    int		i;
	    int		flip = 0;

	    i = bv->ints.value;
	    if (i < 0)
	    {
		i = -i;
		flip = 1;
	    }
	    p = av;
	    result = One;
	    while (i) {
		if (exception)
		    RETURN (Zero);
		if (i & 1)
		    result = Times (result, p);
		i >>= 1;
		if (i)
		    p = Times (p, p);
	    }
	    if (flip)
		result = Divide (One, result);
	}
	break;
    case type_integer:
	{
	    Value   p;
	    Natural *i;
	    Natural *two;
	    Natural *rem;
	    int	    flip = 0;

	    i = bv->integer.mag;
	    if (bv->integer.sign == Negative)
		flip = 1;
	    two = NewNatural (2);
	    p = av;
	    result = One;
	    while (!NaturalZero (i)) {
		if (exception)
		    RETURN (Zero);
		if (!NaturalEven (i))
		    result = Times (result, p);
		i = NaturalDivide (i, two, &rem);
		if (!NaturalZero (i))
		    p = Times (p, p);
	    }
	    if (flip)
		result = Divide (One, result);
	}
	break;
    default:
	{
	    av = DoubleType.promote (av);
	    bv = DoubleType.promote (bv);
	    result = NewDouble (pow (av->doubles.value, bv->doubles.value));
	}
    }
    RETURN (result);
}

Value
Gcd (Value av, Value bv)
{
    ENTER ();
    
    if (!Integralp (av->value.tag) || !Integralp (bv->value.tag))
    {
	RaiseError ("undefined type combination gcd(%T, %T)",
		av->value.tag, bv->value.tag);
	RETURN (Zero);
    }
    RETURN (Reduce (NewInteger (Positive, 
				NaturalGcd (IntegerType.promote (av)->integer.mag,
								 IntegerType.promote (bv)->integer.mag))));
}

StackObject *ValuePrintStack;
int	    ValuePrintLevel;

Bool
Print (Value f, Value v, char format, int base, int width, int prec, unsigned char fill)
{
    int	    i;
    Bool    ret;
    
    if (!v->value.type->print)
	return True;
    for (i = 0; i < ValuePrintLevel; i++)
    {
	if (STACK_ELT(ValuePrintStack, i) == v)
	{
	    FilePuts (f, "(recursive)");
	    return True;
	}
    }
    STACK_PUSH (ValuePrintStack, v);
    ++ValuePrintLevel;
    ret = (*v->value.type->print) (f, v, format, base, width, prec, fill);
    STACK_POP (ValuePrintStack);
    --ValuePrintLevel;
    return ret;
}

Value
Copy (Value v, Type t)
{
    ENTER ();
    Value   nv;
    int	    i;
    
    switch (t) {
    case type_array:
	if (!v->array.values->constant)
	{
	    nv = NewArray (False, v->array.type, v->array.ndim, v->array.dim);
	    for (i = 0; i < v->array.ents; i++)
		BoxValue (nv->array.values, i) = Copy (BoxValue (v->array.values, i),
						       BoxType (v->array.values, i));
	    v = nv;
	}
	break;
    case type_struct:
	if (!v->structs.values->constant)
	{
	    nv = NewStruct (v->structs.type, False);
	    for (i = 0; i < v->structs.type->nelements; i++)
		BoxValue (nv->structs.values, i) = Copy (BoxValue (v->structs.values, i),
							 BoxType (v->structs.values, i));
	    v = nv;
	}
	break;
    case type_double:
	v = DoubleType.promote (v);
	break;
    default:
	break;
    }
    RETURN (v);
}

Value
Default (Type t)
{
    switch (t) {
    default:
	return Zero;
    case type_string:
	return Blank;
    case type_array:
	return Empty;
    case type_struct:
	return Elementless;
    }
}

int
ValueInit (void)
{
    if (!AtomInit ())
	return 0;
    if (!ArrayInit ())
	return 0;
    if (!FileInit ())
	return 0;
    if (!IntInit ())
	return 0;
    if (!NaturalInit ())
	return 0;
    if (!RationalInit ())
	return 0;
    if (!StringInit ())
	return 0;
    if (!StructInit ())
	return 0;
    ValuePrintStack = StackCreate ();
    MemAddRoot (ValuePrintStack);
    ValuePrintLevel = 0;
    return 1;
}

