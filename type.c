/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * type.c
 *
 * manage datatype
 */

#include	"nickle.h"
#include	"gram.h"

Type		*typePoly;
Type		*typeGroup;
Type		*typeField;
Type		*typeRefPoly;
Type		*typeNil;
Type		*typePrim[rep_void + 1];

static void
TypeNameMark (void *object)
{
    TypeName	*tn = object;

    MemReference (tn->type);
}

static void
TypeRefMark (void *object)
{
    TypeRef	*tr = object;

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
TypeFuncMark (void *object)
{
    TypeFunc	*tf = object;

    MemReference (tf->ret);
    MemReference (tf->args);
}

static void
TypeArrayMark (void *object)
{
    TypeArray	*ta = object;

    MemReference (ta->type);
    MemReference (ta->dimensions);
}

static void
TypeStructMark (void *object)
{
    TypeStruct	*ts = object;

    MemReference (ts->structs);
}

DataType    TypePrimType = { 0, 0 };
DataType    TypeNameType = { TypeNameMark, 0 };
DataType    TypeRefType = { TypeRefMark, 0 };
DataType    ArgTypeType = { ArgTypeMark, 0 };
DataType    TypeFuncType = { TypeFuncMark, 0 };
DataType    TypeArrayType = { TypeArrayMark, 0 };
DataType    TypeStructType = { TypeStructMark, 0 };
DataType    TypeUnitType = { 0, 0 };

static Type *
NewTypePrim (Rep prim)
{
    ENTER ();
    Type   *t;

    t = ALLOCATE (&TypePrimType, sizeof (TypePrim));
    t->base.tag = type_prim;
    t->prim.prim = prim;
    RETURN (t);
}

Type *
NewTypeName (ExprPtr name, Type *type)
{
    ENTER ();
    Type   *t;

    t = ALLOCATE (&TypeNameType, sizeof (TypeName));
    t->base.tag = type_name;
    t->name.expr = name;
    t->name.type = type;
    RETURN (t);
}

Type *
NewTypeRef (Type *ref)
{
    ENTER ();
    Type   *t;

    if (!ref)
	RETURN (0);
    t = ALLOCATE (&TypeRefType, sizeof (TypeRef));
    t->base.tag = type_ref;
    t->ref.ref = ref;
    RETURN (t);
}

ArgType *
NewArgType (Type *type, Bool varargs, Atom name, SymbolPtr symbol, ArgType *next)
{
    ENTER ();
    ArgType *a;

    a = ALLOCATE (&ArgTypeType, sizeof (ArgType));
    a->type = type;
    a->varargs = varargs;
    a->name = name;
    a->symbol = symbol;
    a->next = next;
    RETURN (a);
}

Type *
NewTypeFunc (Type *ret, ArgType *args)
{
    ENTER ();
    Type   *t;

    t = ALLOCATE (&TypeFuncType, sizeof (TypeFunc));
    t->base.tag = type_func;
    t->func.ret = ret;
    t->func.args = args;
    RETURN (t);
}

Type *
NewTypeArray (Type *type, Expr *dimensions)
{
    ENTER ();
    Type   *t;

    t = ALLOCATE (&TypeArrayType, sizeof (TypeArray));
    t->base.tag = type_array;
    t->array.type = type;
    t->array.dimensions = dimensions;
    RETURN (t);
}

Type *
NewTypeStruct (StructType *structs)
{
    ENTER ();
    Type   *t;

    t = ALLOCATE (&TypeStructType, sizeof (TypeStruct));
    t->base.tag = type_struct;
    t->structs.structs = structs;
    t->structs.enumeration = False;
    RETURN (t);
}

Type *
NewTypeUnion (StructType *structs, Bool enumeration)
{
    ENTER ();
    Type   *t;

    t = ALLOCATE (&TypeStructType, sizeof (TypeStruct));
    t->base.tag = type_union;
    t->structs.structs = structs;
    t->structs.enumeration = enumeration;
    RETURN (t);
}

SymbolPtr
TypeNameName (Type *t)
{
    ExprPtr e;
    if (t->base.tag == type_name)
    {
	e = t->name.expr;
	if (e->base.tag == COLONCOLON)
	    e = e->tree.right;
	return e->atom.symbol;
    }
    return 0;
}

Bool
TypeNumeric (Type *t)
{
    if (t == typeGroup)
	return True;
    if (t->base.tag != type_prim)
	return False;
    if (Numericp (t->prim.prim))
	return True;
    return False;
}

Bool
TypeIntegral (Type *t)
{
    if (t == typeGroup)
	return True;
    if (t->base.tag != type_prim)
	return False;
    if (Integralp (t->prim.prim))
	return True;
    return False;
}

Bool
TypeString (Type *t)
{
    if (t->base.tag != type_prim)
	return False;
    if (t->prim.prim == rep_string)
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
TypeCompatible (Type *a, Type *b, Bool contains)
{
    int	    n;
    int	    adim, bdim;
    Bool    ret;
    
    if (a == b)
	return True;
    if (!a || !b)
	return False;

    if (a->base.tag == type_name)
        return TypeCompatible (a->name.type, b, contains);
    if (b->base.tag == type_name)
	return TypeCompatible (a, b->name.type, contains);
    
    if (a->base.tag == type_union)
    {
	StructType  *st = a->structs.structs;
	for (n = 0; n < st->nelements; n++)
	    if (!StructTypeElements(st)[n].name && 
		TypeCompatible (StructTypeElements(st)[n].type, b, contains))
		return True;
    }

    if (b->base.tag == type_union)
    {
	StructType  *st = b->structs.structs;
	for (n = 0; n < st->nelements; n++)
	    if (!StructTypeElements(st)[n].name && 
		TypeCompatible (a, StructTypeElements(st)[n].type, contains))
		return True;
    }
    
    if (TypePoly (a))
	return True;

    if (/* !contains && */ TypePoly (b))
	return True;

    if (a->base.tag != b->base.tag)
	return False;
    switch (a->base.tag) {
    case type_prim:
	if (a->prim.prim == b->prim.prim)
	    return True;
	if (TypeNumeric (a) && TypeNumeric (b))
	    return True;
	break;
    case type_ref:
	/*
	 * Avoid the infinite recursion, but don't unify type yet
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
    case type_func:
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
    case type_array:
	adim = TypeCountDimensions (a->array.dimensions);
	bdim = TypeCountDimensions (b->array.dimensions);
	if (adim == 0 || bdim == 0 || adim == bdim)
	    return TypeCompatible (a->array.type, b->array.type, contains);
	break;
    case type_struct:
    case type_union:
	if (!contains && a->structs.structs->nelements != b->structs.structs->nelements)
	    break;
	/*
	 * Is 'b' a subtype of 'a'?
	 */
	for (n = 0; n < a->structs.structs->nelements; n++)
	{
	    StructElement   *ae;
	    Type	    *bt;

	    ae = &StructTypeElements(a->structs.structs)[n];
	    bt = StructMemType (b->structs.structs, ae->name);
	    if (!bt)
		break;
	    if (!TypeCompatible (ae->type, bt, contains))
		break;
	}
	if (n != a->structs.structs->nelements)
	{
	    /*
	     * is 'a' a subtype of 'b'?
	     */
	    for (n = 0; n < b->structs.structs->nelements; n++)
	    {
		StructElement   *be;
		Type		*at;
    
		be = &StructTypeElements(b->structs.structs)[n];
		at = StructMemType (a->structs.structs, be->name);
		if (!at)
		    break;
		if (!TypeCompatible (at, be->type, contains))
		    break;
	    }
	    /* nope, neither are subtypes */
	    if (n != b->structs.structs->nelements)
		return False;
	}
	return True;
    default:
	break;
    }
    return False;
	
}

/*
 * return the combined type for an operation
 * on a numeric type which is a group
 */
static Type *
TypeBinaryGroup (Type *left, Type *right)
{
    if (TypePoly (left))
    {
	if (TypePoly (right) || TypeNumeric (right))
	    return typeGroup;
    }
    else if (TypePoly (right))
    {
	if (TypeNumeric (left))
	    return typeGroup;
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
static Type *
TypeBinaryIntegral (Type *left, Type *right)
{
    if (TypePoly (left))
	left = typePrim[rep_integer];
    if (TypePoly (right))
	right = typePrim[rep_integer];
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
static Type *
TypeBinaryField (Type *left, Type *right)
{
    if (TypePoly (left))
    {
	if (TypePoly (right) || TypeNumeric (right))
	    return typeField;
    }
    else if (TypePoly (right))
    {
	if (TypeNumeric (left))
	    return typeField;
    }
    else if (TypeNumeric (left) && TypeNumeric (right))
    {
	if (left->prim.prim < right->prim.prim)
	    left = right;
	if (left->prim.prim < rep_rational)
	    left = typePrim[rep_rational];
	return left;
    }
    return 0;
}

/*
 * Return the type resuting from an div operator,
 * integral for numeric type
 */
static Type *
TypeBinaryDiv (Type *left, Type *right)
{
    if (TypePoly (left))
	left = typePrim[rep_float];
    if (TypePoly (right))
	right = typePrim[rep_float];
    if (TypeNumeric (left) && TypeNumeric (right))
    {
	return typePrim[rep_integer];
    }
    return 0;
}

/*
 * Return the type resuting from an exponentiation operator,
 * 'left' for integral 'right', float otherwise
 */
static Type *
TypeBinaryPow (Type *left, Type *right)
{
    if (TypePoly (left))
	left = typePrim[rep_float];
    if (TypePoly (right))
	right = typePrim[rep_float];
    if (TypeNumeric (left) && TypeNumeric (right))
    {
	if (TypeIntegral (right))
	    return left;
	return typePrim[rep_float];
    }
    return 0;
}

/*
 * Return string if both left and right are strings
 */
static Type *
TypeBinaryString (Type *left, Type *right)
{
    if (TypePoly (left))
	left = typePrim[rep_string];
    if (TypePoly (right))
	right = typePrim[rep_string];
    if (TypeString (left) && TypeString (right))
	return left;
    return 0;
}
		
/*
 * Return reference type resulting from addition/subtraction
 */
static Type *
TypeBinaryRefOff (Type *ref, Type *off)
{
    if (TypePoly (ref))
	ref = typeRefPoly;
    if (TypePoly (off))
	off = typePrim[rep_integer];
    if (ref->base.tag == type_ref && TypeIntegral (off))
	return ref;
    return 0;
}
		
/*
 * Return reference type resulting from subtraction
 */
static Type *
TypeBinaryRefMinus (Type *aref, Type *bref)
{
    if (TypePoly (aref))
	aref = typeRefPoly;
    if (TypePoly (bref))
	bref = typeRefPoly;
    if (aref->base.tag == type_ref && bref->base.tag == type_ref)
	if (TypeCompatible (aref->ref.ref, bref->ref.ref, False))
	    return typePrim[rep_integer];
    return 0;
}
		
/*
 * Return type referenced by ref
 */
static Type *
TypeUnaryRef (Type *ref)
{
    if (TypePoly (ref))
	ref = typeRefPoly;
    if (ref->base.tag == type_ref)
	return ref->ref.ref;
    return 0;
}
		
static Type *
TypeUnaryGroup (Type *type)
{
    if (TypePoly (type))
	return typeGroup;
    else if (TypeNumeric (type))
	return type;
    return 0;
}

static Type *
TypeUnaryIntegral (Type *type)
{
    if (TypePoly (type))
	return typePrim[rep_integer];
    if (TypeIntegral (type))
	return type;
    return 0;
}

/*
 * Indexing a string returns this type
 */
static Type *
TypeUnaryString (Type *type)
{
    if (TypePoly (type))
	type = typePrim[rep_string];
    if (TypeString (type))
	return typePrim[rep_integer];
    return 0;
}
		
/*
 * Rep of an array reference
 */
static Type *
TypeUnaryArray (Type *type)
{
    if (TypePoly (type))
	return typePoly;
    if (type->base.tag == type_array)
	return type->array.type;
    return 0;
}

static void
TypeRemove (Type **ts, int i, int *n)
{
    int	remain = *n - (i+1);
    memmove (ts + i, ts + (i+1), remain * sizeof (ts[0]));
    (*n)--;
}

static Type **
TypeAdd (Type **ts, Type *t, int *n, int *size)
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

static Type *
TypeCombineFlatten (Type **rets, int nret, int sret)
{
    ENTER ();
    Type	*ret;
    StructType	*st;
    int		n, m;

    /*
     * Flatten anonymous unions
     */
    for (n = 0; n < nret; n++)
    {
	Type	*ut;

	ut = rets[n];
	if (ut->base.tag == type_union && 
	    (ut->structs.structs->nelements == 0 ||
	     !StructTypeElements(ut->structs.structs)[0].name))
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
	    RETURN (typePoly);

    if (nret == 0)
	RETURN (0);
    if (nret == 1)
	RETURN (rets[0]);
    
    ret = NewTypeUnion (NewStructType (nret), False);
    st = ret->structs.structs;
    for (nret = 0; nret < st->nelements; nret++)
	StructTypeElements(st)[nret].type = rets[nret];
    RETURN(ret);
}

Type *
TypeCombineBinary (Type *left, int tag, Type *right)
{
    ENTER ();
#define NUM_TYPE_STACK	100
    Type    *retsStack[NUM_TYPE_STACK];
    Type    **rets = retsStack;
    Type    *ret;
    int	    nret = 0;
    int	    sret = NUM_TYPE_STACK;
    int	    n;

    if (!left || !right)
	RETURN(0);
    
    if (left->base.tag == type_name)
	RETURN (TypeCombineBinary (left->name.type, tag, right));
    if (right->base.tag == type_name)
	RETURN (TypeCombineBinary (left, tag, right->name.type));
    
    if (left->base.tag == type_union)
    {
        StructType  *st = left->structs.structs;
	for (n = 0; n < st->nelements; n++)
	{
	    if (!StructTypeElements(st)[n].name)
	    {
		ret = TypeCombineBinary (StructTypeElements(st)[n].type, tag, right);
		if (ret)
		    rets = TypeAdd (rets, ret, &nret, &sret);
	    }
	}
    }

    if (right->base.tag == type_union)
    {
        StructType  *st = right->structs.structs;
	for (n = 0; n < st->nelements; n++)
	{
	    if (!StructTypeElements(st)[n].name)
	    {
		ret = TypeCombineBinary (left, tag, StructTypeElements(st)[n].type);
		if (ret)
		    rets = TypeAdd (rets, ret, &nret, &sret);
	    }
	}
    }
    switch (tag) {
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
	if (tag == MINUS && (ret = TypeBinaryRefMinus (left, right)))
	    rets = TypeAdd (rets, ret, &nret, &sret);
	if (tag == MINUS || tag == PLUS)
	{
	    if ((ret = TypeBinaryRefOff (right, left)))
		rets = TypeAdd (rets, ret, &nret, &sret);
	}
	/* fall through ... */
    case TIMES:
    case MOD:
    case ASSIGNTIMES:
    case ASSIGNMOD:
	if ((ret = TypeBinaryGroup (left, right)))
	    rets = TypeAdd (rets, ret, &nret, &sret);
	break;
    case DIV:
    case ASSIGNDIV:
	if ((ret = TypeBinaryDiv (left, right)))
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
	if (TypePoly (left))
	{
	    rets = TypeAdd (rets, typePoly, &nret, &sret);
	}
	else if (TypePoly (right))
	{
	    rets = TypeAdd (rets, typePoly, &nret, &sret);
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
    case AND:
    case OR:
	if ((TypePoly (left) || TypeBool (left)) &&
	    (TypePoly (right) || TypeBool (right)))
	{
	    rets = TypeAdd (rets, typePrim[rep_bool], &nret, &sret);
	}
	break;
    case EQ:
    case NE:
#if 0
    	rets = TypeAdd (rets, typePrim[rep_bool], &nret, &sret);
	break;
#endif
    case LT:
    case GT:
    case LE:
    case GE:
	if (TypeCompatible (left, right, False))
	    rets = TypeAdd (rets, typePrim[rep_bool], &nret, &sret);
	break;
    }
    RETURN (TypeCombineFlatten (rets, nret, sret));
}

Type *
TypeCombineUnary (Type *type, int tag)
{
    ENTER ();
#define NUM_TYPE_STACK	100
    Type    *retsStack[NUM_TYPE_STACK];
    Type    **rets = retsStack;
    Type    *ret;
    int	    nret = 0;
    int	    sret = NUM_TYPE_STACK;
    int	    n;

    /* Avoid error cascade */
    if (!type)
	RETURN(typePoly);

    if (type->base.tag == type_name)
	RETURN(TypeCombineUnary (type->name.type, tag));
    
    if (type->base.tag == type_union)
    {
	StructType  *st = type->structs.structs;
	for (n = 0; n < st->nelements; n++)
	{
	    if (!StructTypeElements(st)[n].name)
	    {
		ret = TypeCombineUnary (StructTypeElements(st)[n].type, tag);
		if (ret)
		    rets = TypeAdd (rets, ret, &nret, &sret);
	    }
	}
    }
    switch (tag) {
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
	if (TypePoly (type) || TypeBool (type))
	    rets = TypeAdd (rets, typePrim[rep_bool], &nret, &sret);
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

Type *
TypeCombineArray (Type *type, int ndim, Bool lvalue)
{
    ENTER ();
#define NUM_TYPE_STACK	100
    Type    *retsStack[NUM_TYPE_STACK];
    Type    **rets = retsStack;
    Type    *ret;
    int	    nret = 0;
    int	    sret = NUM_TYPE_STACK;
    int	    n;

    /* Avoid error cascade */
    if (!type)
	RETURN(typePoly);

    if (type->base.tag == type_name)
	RETURN(TypeCombineArray (type->name.type, ndim, lvalue));
    
    if (type->base.tag == type_union)
    {
	StructType  *st = type->structs.structs;
	for (n = 0; n < st->nelements; n++)
	{
	    if (!StructTypeElements(st)[n].name)
	    {
		ret = TypeCombineArray (StructTypeElements(st)[n].type, ndim, lvalue);
		if (ret)
		    rets = TypeAdd (rets, ret, &nret, &sret);
	    }
	}
    }

    ret = TypeUnaryString (type);
    if (ret)
        rets = TypeAdd (rets, ret, &nret, &sret);

    if (TypePoly (type))
	rets = TypeAdd (rets, typePoly, &nret, &sret);

    if (type->base.tag == type_array)
    {
	n = TypeCountDimensions (type->array.dimensions);
	if (n == 0 || n == ndim)
	    rets = TypeAdd (rets, type->array.type, &nret, &sret);
    }
    RETURN (TypeCombineFlatten (rets, nret, sret));
}

Type *
TypeCombineStruct (Type *type, int tag, Atom atom)
{
    if (!type)
	return 0;

    if (TypePoly (type))
	return typePoly;
	
    if (type->base.tag == type_name)
	return TypeCombineStruct (type->name.type, tag, atom);
	
    switch (tag) {
    case DOT:
	if (type->base.tag == type_struct || type->base.tag == type_union)
	    return StructMemType (type->structs.structs, atom);
	break;
    case ARROW:
	if (type->base.tag == type_ref)
	    return TypeCombineStruct (type->ref.ref, DOT, atom);
	break;
    }
    return 0;
}

Type *
TypeCombineReturn (Type *type)
{
    if (TypePoly (type))
	return typePoly;
	
    if (type->base.tag == type_name)
	return TypeCombineReturn (type->name.type);

    if (type->base.tag == type_func)
	return type->func.ret;

    return 0;
}

Type *
TypeCombineFunction (Type *type)
{
    if (TypePoly (type))
	return typePoly;
	
    if (type->base.tag == type_name)
	return TypeCombineFunction (type->name.type);

    if (type->base.tag == type_func)
	return type;

    return 0;
}

/*
 * Check an assignment for type compatibility; Lvalues can assert
 * maximal domain for their values
 */

Bool
TypeCompatibleAssign (TypePtr a, Value b)
{
    int	adim, bdim;
    int	n;
    
    if (!a || !b)
	return True;

    if (a->base.tag == type_union)
    {
	StructType  *st = a->structs.structs;
	for (n = 0; n < st->nelements; n++)
	    if (!StructTypeElements(st)[n].name && 
		TypeCompatibleAssign (StructTypeElements(st)[n].type, b))
		return True;
    }

    if (TypePoly (a))
	return True;
    
    switch (a->base.tag) {
    case type_prim:
	if (a->prim.prim == ValueTag(b))
	    return True;
	if (Numericp (a->prim.prim) && Numericp (ValueTag(b)))
	{
	    if (a->prim.prim >= ValueTag(b))
		return True;
	}
	break;
    case type_name:
	return TypeCompatibleAssign (a->name.type, b);
    case type_ref:
	if (ValueIsRef(b))
	{
	    if (RefValueGet (b))
		return TypeCompatibleAssign (a->ref.ref, RefValueGet (b));
	    else
		return TypeCompatible (a->ref.ref, RefType (b), True);
	}
	if (ValueIsInt(b) && b->ints.value == 0)
	    return True;
	break;
    case type_func:
	if (ValueIsFunc(b))
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
    case type_array:
	if (ValueIsArray(b))
	{
	    adim = TypeCountDimensions (a->array.dimensions);
	    bdim = b->array.ndim;
	    if (adim == 0 || adim == bdim)
	    {
		if (TypePoly (a->array.type))
		    return True;
		if (TypePoly (b->array.type))
		{
		    int	i;
		    BoxPtr  box = b->array.values;

		    for (i = 0; i < b->array.ents; i++)
			if (BoxValueGet (box, i) &&
			    !TypeCompatibleAssign (a->array.type,
						   BoxValueGet (box, i)))
			{
			    return False;
			}
		    return True;
		}
		else
		    return TypeCompatible (a->array.type, b->array.type, True);
	    }
	}
	break;
    case type_struct:
    case type_union:
	if ((ValueIsStruct(b) && a->base.tag == type_struct) ||
	    (ValueIsUnion(b) && a->base.tag == type_union))
	{
	    for (n = 0; n < a->structs.structs->nelements; n++)
	    {
		StructElement   *ae;
		Type		*bt;
    
		ae = &StructTypeElements(a->structs.structs)[n];
		bt = StructMemType (b->structs.type, ae->name);
		if (!bt)
		    break;
		if (!TypeCompatible (ae->type, bt, True))
		    break;
	    }
	    if (n == a->structs.structs->nelements)
		return True;
	}
	break;
    default:
	break;
    }
    return False;
}

Type *
TypeCanon (Type *type)
{
    if (type && type->base.tag == type_name)
	return TypeCanon (type->name.type);
    return type;
}

int
TypeInit (void)
{
    ENTER ();
    Rep	t;
    StructType	*st;

    for (t = rep_int; t <= rep_void; t++)
    {
	typePrim[t] = NewTypePrim (t);
	MemAddRoot (typePrim[t]);
    }
    typePoly = NewTypePrim(rep_undef);
    MemAddRoot (typePoly);
    typeRefPoly = NewTypeRef (typePoly);
    MemAddRoot (typeRefPoly);
    
    st = NewStructType (3);
    StructTypeElements(st)[0].type = typePrim[rep_integer];
    StructTypeElements(st)[1].type = typePrim[rep_rational];
    StructTypeElements(st)[2].type = typePrim[rep_float];
    typeGroup = NewTypeUnion (st, False);
    MemAddRoot (typeGroup);
    
    st = NewStructType (2);
    StructTypeElements(st)[0].type = typePrim[rep_rational];
    StructTypeElements(st)[1].type = typePrim[rep_float];
    typeField = NewTypeUnion (st, False);
    MemAddRoot (typeField);
    
    st = NewStructType (2);
    StructTypeElements(st)[0].type = typePrim[rep_int];
    StructTypeElements(st)[1].type = typeRefPoly;
    typeNil = NewTypeUnion (st, False);
    MemAddRoot (typeField);
    
    TypeCheckStack = StackCreate ();
    MemAddRoot (TypeCheckStack);
    TypeCheckLevel = 0;
    EXIT ();
    return 1;
}
