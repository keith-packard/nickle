/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * type.c
 *
 * manage datatypes
 */

#include	<config.h>
#include	"nickle.h"
#include	"gram.h"

Types		*typesPoly;
Types		*typesPrim[type_continuation - type_int + 1];

static void
TypesPrimMark (void *object)
{
}

static void
TypesNameMark (void *object)
{
    TypesName	*tn = object;

    MemReference (tn->type);
}

static void
TypesRefMark (void *object)
{
    TypesRef	*tr = object;

    MemReference (tr->ref);
}

static void
ArgTypeMark (void *object)
{
    ArgType *at = object;

    MemReference (at->type);
    MemReference (at->next);
}

static void
TypesFuncMark (void *object)
{
    TypesFunc	*tf = object;

    MemReference (tf->ret);
    MemReference (tf->args);
}

static void
TypesArrayMark (void *object)
{
    TypesArray	*ta = object;

    MemReference (ta->type);
    MemReference (ta->dimensions);
}

static void
TypesStructMark (void *object)
{
    TypesStruct	*ts = object;

    MemReference (ts->structs);
}

DataType    TypesPrimType = { TypesPrimMark, 0 };
DataType    TypesNameType = { TypesNameMark, 0 };
DataType    TypesRefType = { TypesRefMark, 0 };
DataType    ArgTypeType = { ArgTypeMark, 0 };
DataType    TypesFuncType = { TypesFuncMark, 0 };
DataType    TypesArrayType = { TypesArrayMark, 0 };
DataType    TypesStructType = { TypesStructMark, 0 };

Types *
NewTypesPrim (Type prim)
{
    ENTER ();
    Types   *t;

    if (type_int <= prim && prim <= type_continuation && typesPrim[prim])
	t = typesPrim[prim];
    else
    {
	t = ALLOCATE (&TypesPrimType, sizeof (TypesPrim));
	t->base.tag = types_prim;
	t->prim.prim = prim;
    }
    RETURN (t);
}

Types *
NewTypesName (Atom name, Types *type)
{
    ENTER ();
    Types   *t;

    t = ALLOCATE (&TypesNameType, sizeof (TypesName));
    t->base.tag = types_name;
    t->name.name = name;
    t->name.type = type;
    RETURN (t);
}

Types *
NewTypesRef (Types *ref)
{
    ENTER ();
    Types   *t;

    t = ALLOCATE (&TypesRefType, sizeof (TypesRef));
    t->base.tag = types_ref;
    t->ref.ref = ref;
    RETURN (t);
}

ArgType *
NewArgType (Types *type, Bool varargs, Atom name, ArgType *next)
{
    ENTER ();
    ArgType *a;

    a = ALLOCATE (&ArgTypeType, sizeof (ArgType));
    a->type = type;
    a->varargs = varargs;
    a->name = name;
    a->next = next;
    RETURN (a);
}

Types *
NewTypesFunc (Types *ret, ArgType *args)
{
    ENTER ();
    Types   *t;

    t = ALLOCATE (&TypesFuncType, sizeof (TypesFunc));
    t->base.tag = types_func;
    t->func.ret = ret;
    t->func.args = args;
    RETURN (t);
}

Types *
NewTypesArray (Types *type, Expr *dimensions)
{
    ENTER ();
    Types   *t;

    t = ALLOCATE (&TypesArrayType, sizeof (TypesArray));
    t->base.tag = types_array;
    t->array.type = type;
    t->array.dimensions = dimensions;
    RETURN (t);
}

Types *
NewTypesStruct (StructType *structs)
{
    ENTER ();
    Types   *t;

    t = ALLOCATE (&TypesStructType, sizeof (TypesStruct));
    t->base.tag = types_struct;
    t->structs.structs = structs;
    RETURN (t);
}

Type
BaseType (Types *t)
{
    if (t)
    {
	switch (t->base.tag) {
	case types_prim:
	    return t->prim.prim;
	case types_name:
	    return BaseType (t->name.type);
	case types_ref:
	    return type_ref;
	case types_func:
	    return type_func;
	case types_array:
	    return type_array;
	case types_struct:
	    return type_struct;
	}
    }
    return type_undef;
}

#if 0
/* This function is unused (and may be broken as well) */
Bool
TypeEqual (Types *a, Types *b)
{
    if (a == b)
	return True;
    if (!a || !b)
	return False;
    if (a->base.tag == types_name)
        return TypeEqual (a->name.type, b);
    if (b->base.tag == types_name)
	return TypeEqual (a, b->name.type);
    if (a->base.tag != b->base.tag)
	return False;
    switch (a->base.tag) {
    case types_prim:
	if (a->prim.prim == b->prim.prim)
	    return True;
	break;
    case types_ref:
	return TypeEqual (a->ref.ref, b->ref.ref);
    case types_func:
	if (TypeEqual (a->func.ret, b->func.ret))
	{
	    ArgType *aarg = a->func.args, *barg = b->func.args;

	    while (aarg || barg)
	    {
		if (!barg || !aarg)
		    return False;
		if (barg->varargs != aarg->varargs)
		    return False;
		if (!TypeEqual (barg->type, aarg->type))
		    return False;
		aarg = aarg->next;
		barg = barg->next;
	    }
	    return True;
	}
	break;
    case types_array:
	return True;
    case types_struct:
	return True;
    default:
    }
    return False;
}
#endif

Bool
TypePoly (Types *t)
{
    if (!t || (t->base.tag == types_prim && t->prim.prim == type_undef))
	return True;
    return False;
}

Bool
TypeNumeric (Types *t)
{
    if (t->base.tag != types_prim)
	return False;
    if (Numericp (t->prim.prim) || t->prim.prim == type_undef)
	return True;
    return False;
}

Bool
TypeIntegral (Types *t)
{
    if (t->base.tag != types_prim)
	return False;
    if (Integralp (t->prim.prim) || t->prim.prim == type_undef)
	return True;
    return False;
}

Bool
TypeString (Types *t)
{
    if (t->base.tag != types_prim)
	return False;
    if (t->prim.prim == type_string || t->prim.prim == type_undef)
	return True;
    return False;
}

int
TypeCountDimensions (ExprPtr dims)
{
    int	ndim = 0;
    while (dims)
    {
	ndim++;
	dims = dims->tree.right;
    }
    return ndim;
}

StackObject *TypeCheckStack;
int	    TypeCheckLevel;

Bool
TypeCompatible (Types *a, Types *b, Bool contains)
{
    int	    n;
    int	    adim, bdim;
    Bool    ret;
    
    if (a == b)
	return True;
    if (TypePoly (a) || TypePoly (b))
	return True;
    if (a->base.tag == types_name)
        return TypeCompatible (a->name.type, b, contains);
    if (b->base.tag == types_name)
	return TypeCompatible (a, b->name.type, contains);
    if (a->base.tag != b->base.tag)
	return False;
    switch (a->base.tag) {
    case types_prim:
	if (a->prim.prim == b->prim.prim)
	    return True;
	if (TypeNumeric (a) && TypeNumeric (b))
	    return contains ? a->prim.prim >= b->prim.prim : True;
	break;
    case types_ref:
	/*
	 * Avoid the infinite recursion, but don't unify types yet
	 */
	for (n = 0; n < TypeCheckLevel; n++)
	    if (STACK_ELT(TypeCheckStack, n) == a)
		return True;
	STACK_PUSH (TypeCheckStack, a);
	++TypeCheckLevel;
	ret = TypeCompatible (a->ref.ref, b->ref.ref, contains);
	STACK_POP (TypeCheckStack);
	--TypeCheckLevel;
	return ret;
    case types_func:
	if (TypeCompatible (a->func.ret, b->func.ret, contains))
	{
	    ArgType *aarg = a->func.args, *barg = b->func.args;

	    while (aarg || barg)
	    {
		if (!barg || !aarg)
		    return False;
		if (barg->varargs != aarg->varargs)
		    return False;
		if (!TypeCompatible (barg->type, aarg->type, contains))
		    return False;
		aarg = aarg->next;
		barg = barg->next;
	    }
	    return True;
	}
	break;
    case types_array:
	adim = TypeCountDimensions (a->array.dimensions);
	bdim = TypeCountDimensions (b->array.dimensions);
	if (adim == 0 || bdim == 0 || adim == bdim)
	    return TypeCompatible (a->array.type, b->array.type, contains);
	break;
    case types_struct:
	if (!contains && a->structs.structs->nelements != b->structs.structs->nelements)
	    break;
	for (n = 0; n < a->structs.structs->nelements; n++)
	{
	    StructElement   *ae;
	    Types	    *bt;

	    ae = &StructTypeElements(a->structs.structs)[n];
	    bt = StructTypes (b->structs.structs, ae->name);
	    if (!bt)
		break;
	    if (!TypeCompatible (ae->type, bt, contains))
		break;
	}
	if (n != a->structs.structs->nelements)
	    break;
	return True;
    default:
    }
    return False;
	
}

Types *
TypeCombineAssign (Types *left, int tag, Types *right)
{
    if (TypePoly (left) || TypePoly (right))
	return typesPoly;
    
    if (left->base.tag == types_name)
	return TypeCombineAssign (left->name.type, tag, right);
    if (right->base.tag == types_name)
	return TypeCombineAssign (left, tag, right->name.type);
    
    switch (tag) {
    case ASSIGN:
	if (TypeCompatible (left, right, True))
	    return left;
	break;
    case ASSIGNPLUS:
	if (TypeString (left) && TypeString (right))
	    return left;
    case ASSIGNMINUS:
	if (left->base.tag == types_ref && TypeNumeric (right))
	    return left;
	/* fall through...*/
    case ASSIGNTIMES:
    case ASSIGNDIVIDE:
    case ASSIGNDIV:
    case ASSIGNMOD:
    case ASSIGNPOW:
	if (TypeNumeric (left) && TypeNumeric (right))
	{
	    if (left->prim.prim > right->prim.prim)
		return left;
	    else
		return right;
	}
	break;
    case ASSIGNSHIFTL:
    case ASSIGNSHIFTR:
    case ASSIGNLXOR:
    case ASSIGNLAND:
    case ASSIGNLOR:
	if (TypeIntegral (left) && TypeIntegral (right))
	{
	    if (left->prim.prim > right->prim.prim)
		return left;
	    else
		return right;
	}
	break;
    }
    return 0;
}

Types *
TypeCombineBinary (Types *left, int tag, Types *right)
{
    if (TypePoly (left) || TypePoly (right))
	return typesPoly;

    if (left->base.tag == types_name)
	return TypeCombineBinary (left->name.type, tag, right);
    if (right->base.tag == types_name)
	return TypeCombineBinary (left, tag, right->name.type);
    
    switch (tag) {
    case PLUS:
	if (TypeString (left) && TypeString (right))
	    return left;
    case MINUS:
	if (left->base.tag == types_ref && TypeNumeric (right))
	    return left;
	if (TypeNumeric (left) && right->base.tag == types_ref)
	    return right;
    case TIMES:
    case DIV:
    case MOD:
	if (TypeNumeric (left) && TypeNumeric (right))
	{
	    if (TypePoly (left) || TypePoly (right))
		return typesPoly;
	    if (left->prim.prim > right->prim.prim)
		return left;
	    else
		return right;
	}
	break;
    case POW:
	if (TypeNumeric (left) && TypeNumeric (right))
	{
	    if (TypePoly (left) || TypePoly (right))
		return typesPoly;
	    if (right->prim.prim >= type_rational)
	    {
		if (left->prim.prim < type_float)
		    return typesPrim[type_float];
		else
		    return left;
	    }
	    else
	    {
		if (left->prim.prim > right->prim.prim)
		    return left;
		else
		    return right;
	    }
	}
	break;
    case DIVIDE:
	if (TypeNumeric (left) && TypeNumeric (right))
	{
	    if (TypePoly (left) || TypePoly (right))
		return typesPoly;
	    if (left->prim.prim > right->prim.prim)
		right = left;
	    if (right->prim.prim < type_rational)
		return typesPrim[type_rational];
	    else
		return right;
	}
	break;
    case SHIFTL:
    case SHIFTR:
    case LXOR:
    case LAND:
    case LOR:
	if (TypeIntegral (left) && TypeIntegral (right))
	{
	    if (TypePoly (left) || TypePoly (right))
		return typesPoly;
	    if (left->prim.prim > right->prim.prim)
		return left;
	    else
		return right;
	}
	break;
    case COLON:
    case AND:
    case OR:
	if (TypeCompatible (left, right, False))
	{
	    if (TypeNumeric (left) && TypeNumeric (right))
	    {
		if (left->prim.prim > right->prim.prim)
		    return left;
		else
		    return right;
	    }
	    return left;
	}
	break;
    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
	if (TypeCompatible (left, right, False))
	    return typesPrim[type_integer];
	break;
    }
    return 0;
}

Types *
TypeCombineUnary (Types *type, int tag)
{
    if (TypePoly (type))
	return typesPoly;
    
    if (type->base.tag == types_name)
	return TypeCombineUnary (type->name.type, tag);
    
    switch (tag) {
    case STAR:
	if (type->base.tag == types_ref)
	    return type->ref.ref;
	break;
    case UMINUS:
    case LNOT:
    case BANG:
    case FACT:
	if (type->base.tag == types_prim && Numericp (type->prim.prim))
	    return type;
	break;
    case OS:
	if (type->base.tag == types_array)
	    return type->array.type;
	if (type->base.tag == types_prim && type->prim.prim == type_string)
	    return typesPrim[type_integer];
	break;
    }
    return 0;
}

Types *
TypeCombineStruct (Types *type, int tag, Atom atom)
{
    if (TypePoly (type))
	return typesPoly;
	
    if (type->base.tag == types_name)
	return TypeCombineStruct (type->name.type, tag, atom);
	
    switch (tag) {
    case DOT:
	if (type->base.tag == types_struct)
	{
	    return StructTypes (type->structs.structs, atom);
	}
	break;
    case ARROW:
	if (type->base.tag == types_ref)
	    return TypeCombineStruct (type->ref.ref, DOT, atom);
	break;
    }
    return 0;
}

Types *
TypeCombineReturn (Types *type)
{
    if (TypePoly (type))
	return typesPoly;
	
    if (type->base.tag == types_name)
	return TypeCombineFunction (type->name.type);

    if (type->base.tag == types_func)
	return type->func.ret;

    return 0;
}

Types *
TypeCombineFunction (Types *type)
{
    if (TypePoly (type))
	return typesPoly;
	
    if (type->base.tag == types_name)
	return TypeCombineFunction (type->name.type);

    if (type->base.tag == types_func)
	return type;

    return 0;
}

/*
 * Check an assignment for type compatibility; Lvalues can assert
 * maximal domain for their values
 */

Bool
TypeCompatibleAssign (TypesPtr a, Value b, Bool shallow)
{
    int	adim, bdim;
    int	n;
    
    if (TypePoly (a))
	return True;
    
    switch (a->base.tag) {
    case types_prim:
	if (a->prim.prim == b->value.tag)
	    return True;
	if (Numericp (a->prim.prim) && Numericp (b->value.tag))
	{
	    if (a->prim.prim >= b->value.tag)
		return True;
	}
	break;
    case types_name:
	return TypeCompatibleAssign (a->name.type, b, False);
    case types_ref:
	if (b->value.tag == type_ref)
	{
	    /* avoid looping through data structures */
	    if (shallow)
		return True;
	    return TypeCompatibleAssign (a->ref.ref, RefValue (b), True);
	}
	if (b->value.tag == type_int && b->ints.value == 0)
	    return True;
	break;
    case types_func:
	if (b->value.tag == type_func)
	{
	    if (TypeCompatible (a->func.ret,
				b->func.code->base.type, True))
	    {
		ArgType *aarg = a->func.args, *barg = b->func.code->base.args;
    
		while (aarg || barg)
		{
		    if (!barg || !aarg)
			return False;
		    if (barg->varargs != aarg->varargs)
			return False;
		    if (!TypeCompatible (barg->type, aarg->type, True))
			return False;
		    aarg = aarg->next;
		    barg = barg->next;
		}
		return True;
	    }
	}
	break;
    case types_array:
	adim = TypeCountDimensions (a->array.dimensions);
	bdim = b->array.ndim;
	if (adim == 0 || adim == bdim)
	    return TypeCompatible (a->array.type, b->array.type, True);
	break;
    case types_struct:
	for (n = 0; n < a->structs.structs->nelements; n++)
	{
	    StructElement   *ae;
	    Types	    *bt;

	    ae = &StructTypeElements(a->structs.structs)[n];
	    bt = StructTypes (b->structs.type, ae->name);
	    if (!bt)
		break;
	    if (!TypeCompatible (ae->type, bt, True))
		break;
	}
	if (n == a->structs.structs->nelements)
	    return True;
	break;
    default:	
    }
    return False;
}

Types *
TypesCanon (Types *type)
{
    if (type && type->base.tag == types_name)
	return TypesCanon (type->name.type);
    return type;
}

int
TypesInit (void)
{
    ENTER ();
    Type    t;

    for (t = type_int; t <= type_continuation; t++)
    {
	typesPrim[t] = NewTypesPrim (t);
	MemAddRoot (typesPrim[t]);
    }
    typesPoly = NewTypesPrim(type_undef);
    MemAddRoot (typesPoly);
    TypeCheckStack = StackCreate ();
    MemAddRoot (TypeCheckStack);
    TypeCheckLevel = 0;
    EXIT ();
    return 1;
}
