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

static void
TypeTypesMark (void *object)
{
    TypeTypes	*type = object;
    MemReference (type->elt);
}

DataType    TypePrimType = { 0, 0, "TypePrimType" };
DataType    TypeNameType = { TypeNameMark, 0, "TypeNameType" };
DataType    TypeRefType = { TypeRefMark, 0, "TypeRefType" };
DataType    ArgTypeType = { ArgTypeMark, 0, "ArgTypeType" };
DataType    TypeFuncType = { TypeFuncMark, 0, "TypeFuncType" };
DataType    TypeArrayType = { TypeArrayMark, 0, "TypeArrayType" };
DataType    TypeStructType = { TypeStructMark, 0, "TypeStructType" };
DataType    TypeUnitType = { 0, 0, "TypeUnitType" };
DataType    TypeTypesType = { TypeTypesMark, 0, "TypeTypesType" };

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

Type *
NewTypeTypes (TypeElt *elt)
{
    ENTER ();
    Type    *t;

    t = ALLOCATE (&TypeTypesType, sizeof (TypeTypes));
    t->base.tag = type_types;
    t->types.elt = elt;
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
    int		n;
    int		adim, bdim;
    Bool	ret;
    StructType	*st;
    
    if (a == b)
	return True;
    if (!a || !b)
	return False;

    if (a->base.tag == type_name)
        return TypeCompatible (a->name.type, b, contains);
    if (b->base.tag == type_name)
	return TypeCompatible (a, b->name.type, contains);
    
    if (a->base.tag == type_types)
    {
	TypeElt	*elt;
	for (elt = a->types.elt; elt; elt = elt->next)
	    if (TypeCompatible (elt->type, b, contains))
		return True;
	return False;
    }

    if (b->base.tag == type_types)
    {
	TypeElt	*elt;
	for (elt = b->types.elt; elt; elt = elt->next)
	    if (TypeCompatible (a, elt->type, contains))
		return True;
	return False;
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
        st = a->structs.structs;
	for (n = 0; n < st->nelements; n++)
	{
	    Type	    *bt;

	    bt = StructMemType (b->structs.structs, StructTypeAtoms(st)[n]);
	    if (!bt)
		break;
	    if (!TypeCompatible (BoxTypesElements(st->types)[n], bt, contains))
		break;
	}
	if (n != a->structs.structs->nelements)
	{
	    /*
	     * is 'a' a subtype of 'b'?
	     */
	    st = b->structs.structs;
	    for (n = 0; n < st->nelements; n++)
	    {
		Type		*at;
    
		at = StructMemType (a->structs.structs, StructTypeAtoms(st)[n]);
		if (!at)
		    break;
		if (!TypeCompatible (at, BoxTypesElements(st->types)[n], contains))
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
	return typePrim[rep_string];
    if (TypeString (type))
	return typePrim[rep_integer];
    return 0;
}
		
/*
 * Type of an array reference
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

/*
 * Comparison a logical operator type
 */
static Type *
TypeUnaryBool (Type *type)
{
    if (TypePoly (type))
	return typePrim[rep_bool];
    if (TypeBool (type))
	return type;
    return 0;
}

static void
TypeEltMark (void *object)
{
    TypeElt *elt = object;
    MemReference (elt->next);
    MemReference (elt->type);
}

DataType    TypeEltType = { TypeEltMark, 0, "TypeEltType" };

static TypeElt *
NewTypeElt (Type *type, TypeElt *next)
{
    ENTER ();
    TypeElt *elt;

    elt = ALLOCATE (&TypeEltType, sizeof (TypeElt));
    elt->type = type;
    elt->next = next;
    RETURN (elt);
}

static Type *
TypeAdd (Type *old, Type *new)
{
    TypeElt **last;
    
    if (new->base.tag == type_types)
    {
	TypeElt	*elt;

	for (elt = new->types.elt; elt; elt = elt->next)
	    old = TypeAdd (old, elt->type);
    }
    else
    {
	if (!old)
	    old = new;
	else if (old != new)
	{
	    if (old->base.tag != type_types)
		old = NewTypeTypes (NewTypeElt (old, 0));
	    for (last = &old->types.elt; *last; last = &(*last)->next)
		if ((*last)->type == new)
		    break;
	    if (!*last)
		*last = NewTypeElt (new, 0);
	}
    }
    return old;
}

static Type *
TypeCombineFlatten (Type *type)
{
    ENTER ();

    if (type && type->base.tag == type_types)
    {
	TypeElt	*n, **p, *m;

	/*
	 * Remove obvious duplicates
	 */
	for (n = type->types.elt; n; n = n->next)
	{
	    p = &n->next;
	    while ((m = *p))
	    {
		if (m->type == n->type)
		    *p = m->next;
		else
		    p = &m->next;
	    }
	}
	/*
	 * Check for a single type and return just that
	 */
	if (!type->types.elt->next)
	    type = type->types.elt->type;
    }
    RETURN(type);
}

Type *
TypeCombineBinary (Type *left, int tag, Type *right)
{
    ENTER ();
    Type    *type;
    Type    *ret = 0;

    if (!left || !right)
	RETURN(0);
    
    if (left->base.tag == type_name)
	RETURN (TypeCombineBinary (left->name.type, tag, right));
    if (right->base.tag == type_name)
	RETURN (TypeCombineBinary (left, tag, right->name.type));
    
    if (left->base.tag == type_types)
    {
	TypeElt	*elt;
	for (elt = left->types.elt; elt; elt = elt->next)
	    if ((type = TypeCombineBinary (elt->type, tag, right)))
		ret = TypeAdd (ret, type);
    }
    else if (right->base.tag == type_types)
    {
	TypeElt	*elt;
	for (elt = right->types.elt; elt; elt = elt->next)
	    if ((type = TypeCombineBinary (left, tag, elt->type)))
		ret = TypeAdd (ret, type);
    }
    else switch (tag) {
    case ASSIGN:
	if (TypeCompatible (left, right, True))
	{
	    if (TypePoly (left))
		ret = TypeAdd (ret, right);
	    else
		ret = TypeAdd (ret, left);
	}
	break;
    case PLUS:
    case ASSIGNPLUS:
	if ((type = TypeBinaryString (left, right)))
	    ret = TypeAdd (ret, type);
	/* fall through ... */
    case MINUS:
    case ASSIGNMINUS:
	if ((type = TypeBinaryRefOff (left, right)))
	    ret = TypeAdd (ret, type);
	if (tag == MINUS && (type = TypeBinaryRefMinus (left, right)))
	    ret = TypeAdd (ret, type);
	if ((tag == MINUS || tag == PLUS) && 
	    (type = TypeBinaryRefOff (right, left)))
	    ret = TypeAdd (ret, type);
	/* fall through ... */
    case TIMES:
    case MOD:
    case ASSIGNTIMES:
    case ASSIGNMOD:
	if ((type = TypeBinaryGroup (left, right)))
	    ret = TypeAdd (ret, type);
	break;
    case DIV:
    case ASSIGNDIV:
	if ((type = TypeBinaryDiv (left, right)))
	    ret = TypeAdd (ret, type);
	break;
    case POW:
    case ASSIGNPOW:
	if ((type = TypeBinaryPow (left, right)))
	    ret = TypeAdd (ret, type);
	break;
    case DIVIDE:
    case ASSIGNDIVIDE:
	if ((type = TypeBinaryField (left, right)))
	    ret = TypeAdd (ret, type);
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
	if ((type = TypeBinaryIntegral (left, right)))
	    ret = TypeAdd (ret, type);
	break;
    case COLON:
	if (TypePoly (left) || TypePoly (right))
	    ret = TypeAdd (ret, typePoly);
	else if (TypeCompatible (left, right, False))
	{
	    if (TypeNumeric (left) && TypeNumeric (right) &&
		left->prim.prim < right->prim.prim)
		ret = TypeAdd (ret, right);
	    else
		ret = TypeAdd (ret, left);
	}
	break;
    case AND:
    case OR:
	if (TypeUnaryBool (left) && TypeUnaryBool (right))
	    ret = TypeAdd (ret, typePrim[rep_bool]);
	break;
    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
	if (TypeCompatible (left, right, False))
	    ret = TypeAdd (ret, typePrim[rep_bool]);
	break;
    }
    RETURN (TypeCombineFlatten (ret));
}

Type *
TypeCombineUnary (Type *type, int tag)
{
    ENTER ();
    Type    *ret = 0;
    Type    *t;

    /* Avoid error cascade */
    if (!type)
	RETURN(typePoly);

    if (type->base.tag == type_name)
	RETURN(TypeCombineUnary (type->name.type, tag));
    
    if (type->base.tag == type_types)
    {
	TypeElt	*elt;
	for (elt = type->types.elt; elt; elt = elt->next)
	    if ((t = TypeCombineUnary (elt->type, tag)))
		ret = TypeAdd (ret, t);
    }
    else switch (tag) {
    case STAR:
	if ((t = TypeUnaryRef (type)))
	    ret = TypeAdd (ret, t);
	break;
    case LNOT:
	if ((t = TypeUnaryIntegral (type)))
	    ret = TypeAdd (ret, t);
	break;
    case UMINUS:
	if ((t = TypeUnaryGroup (type)))
	    ret = TypeAdd (ret, t);
	break;
    case BANG:
	if ((t = TypeUnaryBool (type)))
	    ret = TypeAdd (ret, t);
	break;
    case FACT:
	if ((t = TypeUnaryIntegral (type)))
	    ret = TypeAdd (ret, t);
	break;
    case OS:
	if ((t = TypeUnaryString (type)))
	    ret = TypeAdd (ret, t);
	if ((t = TypeUnaryArray (type)))
	    ret = TypeAdd (ret, t);
	break;
    }
    RETURN (TypeCombineFlatten (ret));
}

Type *
TypeCombineArray (Type *type, int ndim, Bool lvalue)
{
    ENTER ();
    Type    *ret = 0;
    Type    *t;

    /* Avoid error cascade */
    if (!type)
	RETURN(typePoly);

    if (type->base.tag == type_name)
	RETURN(TypeCombineArray (type->name.type, ndim, lvalue));
    
    if (type->base.tag == type_types)
    {
	TypeElt	*elt;
	for (elt = type->types.elt; elt; elt = elt->next)
	    if ((t = TypeCombineArray (elt->type, ndim, lvalue)))
		ret = TypeAdd (ret, t);
    }
    else 
    {
	if ((t = TypeUnaryString (type)))
	    ret = TypeAdd (ret, t);
	
	if (TypePoly (type))
	    ret = TypeAdd (ret, typePoly);

	if (type->base.tag == type_array)
	{
	    int n = TypeCountDimensions (type->array.dimensions);
	    if (n == 0 || n == ndim)
		ret = TypeAdd (ret, type->array.type);
	}
    }
    RETURN (TypeCombineFlatten (ret));
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

    if (a->base.tag == type_types)
    {
	TypeElt	*elt;
	for (elt = a->types.elt; elt; elt = elt->next)
	    if (TypeCompatibleAssign (elt->type, b))
		return True;
	return False;
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
		if (TypePoly (ArrayType(&b->array)))
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
		    return TypeCompatible (a->array.type, 
					   ArrayType(&b->array), True);
	    }
	}
	break;
    case type_struct:
    case type_union:
	if ((ValueIsStruct(b) && a->base.tag == type_struct) ||
	    (ValueIsUnion(b) && a->base.tag == type_union))
	{
	    StructType	*st = a->structs.structs;
	    for (n = 0; n < st->nelements; n++)
	    {
		Type		*bt;
    
		bt = StructMemType (b->structs.type, StructTypeAtoms(st)[n]);
		if (!bt)
		    break;
		if (!TypeCompatible (BoxTypesElements(st->types)[n], bt, True))
		    break;
	    }
	    if (n == st->nelements)
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

    for (t = rep_int; t <= rep_void; t++)
    {
	typePrim[t] = NewTypePrim (t);
	MemAddRoot (typePrim[t]);
    }
    typePoly = NewTypePrim(rep_undef);
    MemAddRoot (typePoly);
    typeRefPoly = NewTypeRef (typePoly);
    MemAddRoot (typeRefPoly);
    
    typeGroup = NewTypeTypes (NewTypeElt (typePrim[rep_integer],
					  NewTypeElt (typePrim[rep_rational],
						      NewTypeElt (typePrim[rep_float], 0))));
    MemAddRoot (typeGroup);
    
    typeField = NewTypeTypes (NewTypeElt (typePrim[rep_rational],
					  NewTypeElt (typePrim[rep_float], 0)));
    MemAddRoot (typeField);
    
    TypeCheckStack = StackCreate ();
    MemAddRoot (TypeCheckStack);
    TypeCheckLevel = 0;
    EXIT ();
    return 1;
}
