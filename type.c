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

#include	"nickle.h"
#include	"gram.h"

Types		*typesPoly;
Types		*typesGroup;
Types		*typesField;
Types		*typesRefPoly;
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

Types *
NewTypesUnion (StructType *structs)
{
    ENTER ();
    Types   *t;

    t = ALLOCATE (&TypesStructType, sizeof (TypesStruct));
    t->base.tag = types_union;
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
	case types_union:
	    return type_union;
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
    if (t == typesGroup)
	return True;
    if (t->base.tag != types_prim)
	return False;
    if (Numericp (t->prim.prim))
	return True;
    return False;
}

Bool
TypeIntegral (Types *t)
{
    if (t == typesGroup)
	return True;
    if (t->base.tag != types_prim)
	return False;
    if (Integralp (t->prim.prim))
	return True;
    return False;
}

Bool
TypeString (Types *t)
{
    if (t->base.tag != types_prim)
	return False;
    if (t->prim.prim == type_string)
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
    
    if (a->base.tag == types_union)
    {
	StructType  *st = a->structs.structs;
	for (n = 0; n < st->nelements; n++)
	    if (TypeCompatible (StructTypeElements(st)[n].type, b, contains))
		return True;
	return False;
    }

    if (b->base.tag == types_union)
    {
	StructType  *st = b->structs.structs;
	for (n = 0; n < st->nelements; n++)
	    if (TypeCompatible (a, StructTypeElements(st)[n].type, contains))
		return True;
	return False;
    }
    
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
    case types_union:
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

/*
 * return the combined type for an operation
 * on a numeric type which is a group
 */
static Types *
TypeBinaryGroup (Types *left, Types *right)
{
    if (TypePoly (left))
    {
	if (TypePoly (right) || TypeNumeric (right))
	    return typesGroup;
    }
    else if (TypePoly (right))
    {
	if (TypeNumeric (left))
	    return typesGroup;
    }
    else if (TypeNumeric (left) &&  TypeNumeric (right))
    {
	if (left->prim.prim < right->prim.prim)
	    left = right;
	return left;
    }
    return 0;
}

/*
 * Return the least-upper bound for an integral computation
 */
static Types *
TypeBinaryIntegral (Types *left, Types *right)
{
    if (TypePoly (left))
	left = typesPrim[type_integer];
    if (TypePoly (right))
	right = typesPrim[type_integer];
    if (TypeIntegral (left) && TypeIntegral (right))
    {
	if (left->prim.prim < right->prim.prim)
	    left = right;
	return left;
    }
    return 0;
}

/*
 * return the combined type for an operation
 * on a set closed under addition and multiplication
 */
static Types *
TypeBinaryField (Types *left, Types *right)
{
    if (TypePoly (left))
    {
	if (TypePoly (right) || TypeNumeric (right))
	    return typesField;
    }
    else if (TypePoly (right))
    {
	if (TypeNumeric (left))
	    return typesField;
    }
    else if (TypeNumeric (left) && TypeNumeric (right))
    {
	if (left->prim.prim < right->prim.prim)
	    left = right;
	if (left->prim.prim < type_rational)
	    left = typesPrim[type_rational];
	return left;
    }
    return 0;
}

/*
 * Return the type resuting from an exponentiation operator,
 * 'left' for integral 'right', float otherwise
 */
static Types *
TypeBinaryPow (Types *left, Types *right)
{
    if (TypePoly (left))
	left = typesPrim[type_float];
    if (TypePoly (right))
	right = typesPrim[type_float];
    if (TypeNumeric (left) && TypeNumeric (right))
    {
	if (TypeIntegral (right))
	    return left;
	return typesPrim[type_float];
    }
    return 0;
}

/*
 * Return string if both left and right are strings
 */
static Types *
TypeBinaryString (Types *left, Types *right)
{
    if (TypePoly (left))
	left = typesPrim[type_string];
    if (TypePoly (right))
	right = typesPrim[type_string];
    if (TypeString (left) && TypeString (right))
	return left;
    return 0;
}
		
/*
 * Return reference type resulting from addition/subtraction
 */
static Types *
TypeBinaryRefOff (Types *ref, Types *off)
{
    if (TypePoly (ref))
	ref = typesRefPoly;
    if (TypePoly (off))
	off = typesPrim[type_integer];
    if (ref->base.tag == types_ref && TypeIntegral (off))
	return ref;
    return 0;
}
		
/*
 * Return type referenced by ref
 */
static Types *
TypeUnaryRef (Types *ref)
{
    if (TypePoly (ref))
	ref = typesRefPoly;
    if (ref->base.tag == types_ref)
	return ref->ref.ref;
    return 0;
}
		
static Types *
TypeUnaryGroup (Types *type)
{
    if (TypePoly)
	return typesGroup;
    else if (TypeNumeric (type))
	return type;
    return 0;
}

static Types *
TypeUnaryIntegral (Types *type)
{
    if (TypePoly)
	return typesPrim[type_integer];
    if (TypeIntegral (type))
	return type;
    return 0;
}

/*
 * Indexing a string returns this type
 */
static Types *
TypeUnaryString (Types *type)
{
    if (TypePoly (type))
	type = typesPrim[type_string];
    if (TypeString (type))
	return typesPrim[type_integer];
    return 0;
}
		
/*
 * Type of an array reference
 */
static Types *
TypeUnaryArray (Types *type)
{
    if (TypePoly (type))
	return typesPoly;
    if (type->base.tag == types_array)
	return type->array.type;
    return 0;
}

static void
TypeRemove (Types **ts, int i, int *n)
{
    int	remain = *n - (i+1);
    memmove (ts + i, ts + (i+1), remain * sizeof (ts[0]));
    (*n)--;
}

static Types **
TypeAdd (Types **ts, Types *t, int *n, int *size)
{
    if (*n >= *size) 
    {
	int ns = *n + 10;
	ts = memmove (AllocateTemp (ns * sizeof (ts[0])),
		      ts,
		      *size * sizeof (ts[0]));
	*size = ns;
    }
    ts[(*n)++] = t;
    return ts;
}

static Types *
TypeCombineFlatten (Types **rets, int nret, int sret)
{
    ENTER ();
    Types	*ret;
    StructType	*st;
    int		n, m;

    /*
     * Flatten unions
     */
    for (n = 0; n < nret; n++)
    {
	Types	*ut;

	ut = rets[n];
	if (ut->base.tag == types_union)
	{
	    st = ut->structs.structs;
	    
	    TypeRemove (rets, n, &nret);
	    for (m = 0; m < st->nelements; m++)
		rets = TypeAdd (rets, StructTypeElements(st)[m].type, &nret, &sret);
	}
    }

    /*
     * Remove obvious duplicates
     */
    for (n = 0; n < nret-1; n++)
	for (m = n+1; m < nret;)
	{
	    if (rets[n] == rets[m])
		TypeRemove (rets,m,&nret);
	    else
		m++;
	}

    /*
     * Check for quick bail
     */
    for (n = 0; n < nret; n++)
	if (TypePoly (rets[n]))
	    RETURN (typesPoly);

    if (nret == 0)
	RETURN (0);
    if (nret == 1)
	RETURN (rets[0]);
    
    ret = NewTypesUnion (NewStructType (nret));
    st = ret->structs.structs;
    for (nret = 0; nret < st->nelements; nret++)
	StructTypeElements(st)[nret].type = rets[nret];
    RETURN(ret);
}

Types *
TypeCombineBinary (Types *left, int tag, Types *right)
{
    ENTER ();
#define NUM_TYPE_STACK	100
    Types   *retsStack[NUM_TYPE_STACK];
    Types   **rets = retsStack;
    Types   *ret;
    int	    nret = 0;
    int	    sret = NUM_TYPE_STACK;
    int	    n;

    /* Avoid error cascade */
    if (!left || !right)
	RETURN(typesPoly);
    if (left->base.tag == types_name)
	RETURN (TypeCombineBinary (left->name.type, tag, right));
    if (right->base.tag == types_name)
	RETURN (TypeCombineBinary (left, tag, right->name.type));
    
    if (left->base.tag == types_union)
    {
        StructType  *st = left->structs.structs;
	for (n = 0; n < st->nelements; n++)
	{
	    ret = TypeCombineBinary (StructTypeElements(st)[n].type, tag, right);
	    if (ret)
		rets = TypeAdd (rets, ret, &nret, &sret);
	}
    }
    else if (right->base.tag == types_union)
    {
        StructType  *st = right->structs.structs;
	for (n = 0; n < st->nelements; n++)
	{
	    ret = TypeCombineBinary (left, tag, StructTypeElements(st)[n].type);
	    if (ret)
		rets = TypeAdd (rets, ret, &nret, &sret);
	}
    } else switch (tag) {
    case ASSIGN:
	if (TypeCompatible (left, right, True))
	{
	    if (TypePoly (left))
		rets = TypeAdd (rets, right, &nret, &sret);
	    else
		rets = TypeAdd (rets, left, &nret, &sret);
	}
	break;
    case PLUS:
    case ASSIGNPLUS:
	if ((ret = TypeBinaryString (left, right)))
	    rets = TypeAdd (rets, ret, &nret, &sret);
	/* fall through ... */
    case MINUS:
    case ASSIGNMINUS:
	if ((ret = TypeBinaryRefOff (left, right)))
	    rets = TypeAdd (rets, ret, &nret, &sret);
	if (tag == MINUS || tag == PLUS)
	{
	    if ((ret = TypeBinaryRefOff (right, left)))
		rets = TypeAdd (rets, ret, &nret, &sret);
	}
	/* fall through ... */
    case TIMES:
    case DIV:
    case MOD:
    case ASSIGNTIMES:
    case ASSIGNDIV:
    case ASSIGNMOD:
	if ((ret = TypeBinaryGroup (left, right)))
	    rets = TypeAdd (rets, ret, &nret, &sret);
	break;
    case POW:
    case ASSIGNPOW:
	if ((ret = TypeBinaryPow (left, right)))
	    rets = TypeAdd (rets, ret, &nret, &sret);
	break;
    case DIVIDE:
    case ASSIGNDIVIDE:
	if ((ret = TypeBinaryField (left, right)))
	    rets = TypeAdd (rets, ret, &nret, &sret);
	break;
    case SHIFTL:
    case SHIFTR:
    case LXOR:
    case LAND:
    case LOR:
    case ASSIGNSHIFTL:
    case ASSIGNSHIFTR:
    case ASSIGNLXOR:
    case ASSIGNLAND:
    case ASSIGNLOR:
	if ((ret = TypeBinaryIntegral (left, right)))
	    rets = TypeAdd (rets, ret, &nret, &sret);
	break;
    case COLON:
    case AND:
    case OR:
	if (TypePoly (left))
	{
	    rets = TypeAdd (rets, typesPoly, &nret, &sret);
	}
	else if (TypePoly (right))
	{
	    rets = TypeAdd (rets, typesPoly, &nret, &sret);
	}
	else if (TypeCompatible (left, right, False))
	{
	    if (TypeNumeric (left) && TypeNumeric (right) &&
		left->prim.prim < right->prim.prim)
		rets = TypeAdd (rets, right, &nret, &sret);
	    else
		rets = TypeAdd (rets, left, &nret, &sret);
	}
	break;
    case EQ:
    	rets = TypeAdd (rets, typesPrim[type_integer], &nret, &sret);
	break;
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
	if (TypeCompatible (left, right, False))
	    rets = TypeAdd (rets, typesPrim[type_integer], &nret, &sret);
	break;
    }
    RETURN (TypeCombineFlatten (rets, nret, sret));
}

Types *
TypeCombineUnary (Types *type, int tag)
{
    ENTER ();
#define NUM_TYPE_STACK	100
    Types   *retsStack[NUM_TYPE_STACK];
    Types   **rets = retsStack;
    Types   *ret;
    int	    nret = 0;
    int	    sret = NUM_TYPE_STACK;
    int	    n;

    /* Avoid error cascade */
    if (!type)
	RETURN(typesPoly);

    if (type->base.tag == types_name)
	RETURN(TypeCombineUnary (type->name.type, tag));
    
    if (type->base.tag == types_union)
    {
	StructType  *st = type->structs.structs;
	for (n = 0; n < st->nelements; n++)
	{
	    ret = TypeCombineUnary (StructTypeElements(st)[n].type, tag);
	    if (ret)
		rets = TypeAdd (rets, ret, &nret, &sret);
	}
    }
    else switch (tag) {
    case STAR:
	ret = TypeUnaryRef (type);
	if (ret)
	    rets = TypeAdd (rets, ret, &nret, &sret);
	break;
    case LNOT:
	ret = TypeUnaryIntegral (type);
	if (ret)
	    rets = TypeAdd (rets, ret, &nret, &sret);
	break;
    case UMINUS:
	ret = TypeUnaryGroup (type);
	if (ret)
	    rets = TypeAdd (rets, ret, &nret, &sret);
	break;
    case BANG:
	ret = TypeCombineBinary (type, EQ, typesPrim[type_integer]);
	if (ret)
	    rets = TypeAdd (rets, ret, &nret, &sret);
	break;
    case FACT:
	ret = TypeUnaryIntegral (type);
	if (ret)
	    rets = TypeAdd (rets, ret, &nret, &sret);
	break;
    case OS:
	ret = TypeUnaryString (type);
	if (ret)
	    rets = TypeAdd (rets, ret, &nret, &sret);
	ret = TypeUnaryArray (type);
	if (ret)
	    rets = TypeAdd (rets, ret, &nret, &sret);
	break;
    }
    RETURN (TypeCombineFlatten (rets, nret, sret));
}

Types *
TypeCombineArray (Types *type, int ndim, Bool lvalue)
{
    ENTER ();
#define NUM_TYPE_STACK	100
    Types   *retsStack[NUM_TYPE_STACK];
    Types   **rets = retsStack;
    Types   *ret;
    int	    nret = 0;
    int	    sret = NUM_TYPE_STACK;
    int	    n;

    /* Avoid error cascade */
    if (!type)
	RETURN(typesPoly);

    if (type->base.tag == types_name)
	RETURN(TypeCombineArray (type->name.type, ndim, lvalue));
    
    if (type->base.tag == types_union)
    {
	StructType  *st = type->structs.structs;
	for (n = 0; n < st->nelements; n++)
	{
	    ret = TypeCombineArray (StructTypeElements(st)[n].type, ndim, lvalue);
	    if (ret)
		rets = TypeAdd (rets, ret, &nret, &sret);
	}
    }

    ret = TypeUnaryString (type);
    if (ret)
        rets = TypeAdd (rets, ret, &nret, &sret);

    if (TypePoly (type))
	rets = TypeAdd (rets, typesPoly, &nret, &sret);

    if (type->base.tag == types_array)
    {
	n = TypeCountDimensions (type->array.dimensions);
	if (n == 0 || n == ndim)
	    rets = TypeAdd (rets, type->array.type, &nret, &sret);
    }
    RETURN (TypeCombineFlatten (rets, nret, sret));
}

Types *
TypeCombineStruct (Types *type, int tag, Atom atom)
{
    if (!type)
	return 0;

    if (TypePoly (type))
	return typesPoly;
	
    if (type->base.tag == types_name)
	return TypeCombineStruct (type->name.type, tag, atom);
	
    switch (tag) {
    case DOT:
	if (type->base.tag == types_struct || type->base.tag == types_union)
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
    
    if (!b)
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
	    return TypeCompatibleAssign (a->ref.ref, RefValueGet (b), True);
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
    case types_union:
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
    Type	t;
    StructType	*st;

    for (t = type_int; t <= type_continuation; t++)
    {
	typesPrim[t] = NewTypesPrim (t);
	MemAddRoot (typesPrim[t]);
    }
    typesPoly = NewTypesPrim(type_undef);
    MemAddRoot (typesPoly);
    typesRefPoly = NewTypesRef (typesPoly);
    MemAddRoot (typesRefPoly);
    
    st = NewStructType (3);
    StructTypeElements(st)[0].type = typesPrim[type_integer];
    StructTypeElements(st)[1].type = typesPrim[type_rational];
    StructTypeElements(st)[2].type = typesPrim[type_float];
    typesGroup = NewTypesUnion (st);
    MemAddRoot (typesGroup);
    
    st = NewStructType (2);
    StructTypeElements(st)[0].type = typesPrim[type_rational];
    StructTypeElements(st)[1].type = typesPrim[type_float];
    typesField = NewTypesUnion (st);
    MemAddRoot (typesField);
    
    TypeCheckStack = StackCreate ();
    MemAddRoot (TypeCheckStack);
    TypeCheckLevel = 0;
    EXIT ();
    return 1;
}
