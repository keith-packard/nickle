/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
Type		*typeRefPoly;
Type		*typeArrayInt;
Type		*typePrim[rep_void + 1];

static void
TypeNameMark (void *object)
{
    TypeName	*tn = object;

    MemReference (tn->name);
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
    switch (ta->storage) {
    case DimStorageNone:
	break;
    case DimStorageGlobal:
	MemReference (ta->u.global);
	break;
    case DimStorageStatic:
    case DimStorageAuto:
	MemReference (ta->u.frame.code);
	break;
    }
}

static void
TypeHashMark (void *object)
{
    TypeHash	*th = object;

    MemReference (th->type);
    MemReference (th->keyType);
}

static void
TypeStructMark (void *object)
{
    TypeStruct	*ts = object;

    MemReference (ts->structs);
    MemReference (ts->left);
    MemReference (ts->right);
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
DataType    TypeHashType = { TypeHashMark, 0, "TypeHashType" };
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
NewTypeName (ExprPtr expr, Symbol *name)
{
    ENTER ();
    Type   *t;

    t = ALLOCATE (&TypeNameType, sizeof (TypeName));
    t->base.tag = type_name;
    t->name.expr = expr;
    t->name.name = name;
    RETURN (t);
}

Type *
NewTypeRef (Type *ref, Bool pointer)
{
    ENTER ();
    Type   *t;

    if (!ref)
	RETURN (0);
    t = ALLOCATE (&TypeRefType, sizeof (TypeRef));
    t->base.tag = type_ref;
    t->ref.ref = ref;
    t->ref.pointer = pointer;
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
NewTypeArray (Type *type, Expr *dimensions, Bool resizable)
{
    ENTER ();
    Type   *t;

    t = ALLOCATE (&TypeArrayType, sizeof (TypeArray));
    t->base.tag = type_array;
    t->array.type = type;
    t->array.dimensions = dimensions;
    t->array.storage = DimStorageNone;
    t->array.resizable = resizable;
    RETURN (t);
}

Type *
NewTypeHash (Type *type, Type *keyType)
{
    ENTER ();
    Type   *t;

    t = ALLOCATE (&TypeHashType, sizeof (TypeHash));
    t->base.tag = type_hash;
    t->hash.type = type;
    t->hash.keyType = keyType;
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
    t->structs.left = NULL;
    t->structs.right = NULL;
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
    t->structs.left = NULL;
    t->structs.right = NULL;
    RETURN (t);
}

static Type *
TypePlusPart (Type *type)
{
    type = TypeCanon (type);
    switch (type->base.tag) {
    case type_struct:
    case type_union:
	break;
    default:
	ParseError ("Type '%T' not struct or union", type);
	return NULL;
    }
    return type;
}

static int
AddPlusType (StructType	*new, StructType *old, int pos)
{
    int	i;

    for (i = 0; i < old->nelements; i++) {
        AddBoxType (&new->types, BoxTypesElements (old->types)[i]);
        StructTypeAtoms (new)[pos] = StructTypeAtoms (old)[i];
        pos++;
    }
    return pos;
}

Type *
NewTypePlus (Type *left, Type *right)
{
    ENTER ();
    Type	*t, *l, *r;;
    StructType	*st;
    int		i;
    
    l = TypePlusPart (left);
    r = TypePlusPart (right);
    if (!l || !r)
	RETURN (NULL);
    if (l->base.tag != r->base.tag) {
	ParseError ("'%T' and '%T' are not the same type", left, right);
	RETURN (NULL);
    }
    
    st = NewStructType (l->structs.structs->nelements + r->structs.structs->nelements);
    i = AddPlusType (st, l->structs.structs, 0);
    if (i < 0)
	RETURN (NULL);
    i = AddPlusType (st, r->structs.structs, i);
    if (i < 0)
	RETURN (NULL);

    t = ALLOCATE (&TypeStructType, sizeof (TypeStruct));
    t->base.tag = l->base.tag;
    t->structs.structs = st;
    t->structs.enumeration = l->structs.enumeration && r->structs.enumeration;
    t->structs.left = left;
    t->structs.right = right;
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
    if (t->base.tag != type_prim)
	return False;
    if (Numericp (t->prim.prim))
	return True;
    return False;
}

Bool
TypeIntegral (Type *t)
{
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

/* 
 * Return True if sup is a super type of sub
 */

Bool
TypeIsSupertype (Type *super, Type *sub)
{
    int		n;
    Bool	ret;
    StructType	*super_st;
    StructType	*sub_st;
    int		super_dim;
    int		sub_dim;

    if (super == sub)
	return True;
    if (!super || !sub)
	return False;

    /* resolve typedefs */
    if (super->base.tag == type_name)
	return TypeIsSupertype (TypeNameType (super), sub);
    if (sub->base.tag ==  type_name)
	return TypeIsSupertype (super, TypeNameType (sub));

    /* check bogus internal union types */
    if (super->base.tag == type_types)
    {
	TypeElt	*elt;

	for (elt = super->types.elt; elt; elt = elt->next)
	    if (TypeIsSupertype (elt->type, sub))
		return True;
	return False;
    }

    if (sub->base.tag == type_types)
    {
	TypeElt	*elt;

	for (elt = sub->types.elt; elt; elt = elt->next)
	    if (TypeIsSupertype (super, elt->type))
		return True;
	return False;
    }

    /* poly is a supertype of all types */
    if (TypePoly (super))
	return True;

    if (super->base.tag != sub->base.tag)
	return False;

    switch (super->base.tag) {
    case type_prim:
	if (super->prim.prim == sub->prim.prim)
	    return True;
	if (Numericp (super->prim.prim) && Numericp (sub->prim.prim))
	    return super->prim.prim >= sub->prim.prim;
	return False;
    case type_ref:
	/*
	 * Avoid the infinite recursion, but don't unify type yet
	 */
	for (n = 0; n < TypeCheckLevel; n++)
	    if (STACK_ELT(TypeCheckStack, n) == super)
		return True;
	STACK_PUSH (TypeCheckStack, super);
	++TypeCheckLevel;
	/* XXX is this right? */
	ret = TypeIsSupertype (super->ref.ref, sub->ref.ref);
	STACK_POP (TypeCheckStack);
	--TypeCheckLevel;
	return ret;
    case type_func:
	if (TypeIsSupertype (super->func.ret, sub->func.ret))
	{
	    ArgType *super_arg = super->func.args;
	    ArgType *sub_arg = sub->func.args;

	    while (super_arg || sub_arg)
	    {
		if (!super_arg || !sub_arg)
		    return False;
		if (super_arg->varargs != sub_arg->varargs)
		    return False;
		if (!TypeIsSupertype (sub_arg->type, super_arg->type))
		    return False;
		super_arg = super_arg->next;
		sub_arg = sub_arg->next;
	    }
	    return True;
	}
	return False;
    case type_array:
	super_dim = TypeCountDimensions (super->array.dimensions);
	sub_dim = TypeCountDimensions (sub->array.dimensions);
	if (super_dim == 0 || sub_dim == 0 || super_dim == sub_dim)
	    return TypeIsSupertype (super->array.type, sub->array.type);
	return False;
    case type_hash:
	return (TypeIsSupertype (super->hash.type, sub->hash.type) &&
		TypeIsOrdered (super->hash.keyType, sub->hash.keyType));
    case type_struct:
    case type_union:
        super_st = super->structs.structs;
	sub_st = sub->structs.structs;
	for (n = 0; n < super_st->nelements; n++)
	{
	    Type	    *sub_mem;

	    /* 
	     * Structs (or unions) are subtypes if they contain all
	     * of the super type members and those members are subtypes
	     */
	    sub_mem = StructMemType (sub_st, StructTypeAtoms(super_st)[n]);
	    if (!sub_mem)
		return False;
	    if (!TypeIsSupertype (BoxTypesElements(super_st->types)[n],
				  sub_mem))
		return False;
	}
	return True;
    case type_name:
    case type_types:
	abort ();
    }
    return False;
}

/*
 * Return True if a is a super or subtype of b
 */

Bool
TypeIsOrdered (Type *a, Type *b)
{
    return TypeIsSupertype (a, b) || TypeIsSupertype (b, a);
}

#if 0

/*
 * The above relationship isn't quite right --
 *
 *	real(real) x = int func(int a) { return a + 1; };
 *
 * fails as int(int) is neither supertype nor subtype of real(real)
 *
 * We're trying to figure out what the right answer is, and for everything
 * aside from structures, it looks pretty easy.  Structs are "hard"...
 */

/* 
 * Return True if a is a "co-type" of b
 */

Bool
TypeIsCotype (Type *a, Type *b)
{
    int		n;
    Bool	ret;
    StructType	*a_st;
    StructType	*b_st;
    int		a_dim;
    int		b_dim;

    if (a == b)
	return True;
    if (!a || !b)
	return False;

    /* resolve typedefs */
    if (a->base.tag == type_name)
	return TypeIsCotype (TypeNameType (a), b);
    if (b->base.tag ==  type_name)
	return TypeIsCotype (a, TypeNameType (b));

    /* check bogus internal union types */
    if (a->base.tag == type_types)
    {
	TypeElt	*elt;

	for (elt = a->types.elt; elt; elt = elt->next)
	    if (TypeIsCotype (elt->type, b))
		return True;
	return False;
    }

    if (b->base.tag == type_types)
    {
	TypeElt	*elt;

	for (elt = b->types.elt; elt; elt = elt->next)
	    if (TypeIsCotype (a, elt->type))
		return True;
	return False;
    }

    /* poly is a supertype of all types */
    if (TypePoly (a) || TypePoly (b))
	return True;

    if (a->base.tag != b->base.tag)
	return False;

    switch (a->base.tag) {
    case type_prim:
	if (a->prim.prim == b->prim.prim)
	    return True;
	if (Numericp (a->prim.prim) && Numericp (b->prim.prim))
	    return True;
	return False;
    case type_ref:
	/*
	 * Avoid the infinite recursion, but don't unify type yet
	 */
	for (n = 0; n < TypeCheckLevel; n++)
	    if (STACK_ELT(TypeCheckStack, n) == a)
		return True;
	STACK_PUSH (TypeCheckStack, a);
	++TypeCheckLevel;
	/* XXX is this right? */
	ret = TypeIsCotype (a->ref.ref, b->ref.ref);
	STACK_POP (TypeCheckStack);
	--TypeCheckLevel;
	return ret;
    case type_func:
	if (TypeIsCotype (a->func.ret, b->func.ret))
	{
	    ArgType *a_arg = a->func.args;
	    ArgType *b_arg = b->func.args;

	    while (a_arg || b_arg)
	    {
		if (!a_arg || !b_arg)
		    return False;
		if (a_arg->varargs != b_arg->varargs)
		    return False;
		if (!TypeIsCotype (b_arg->type, a_arg->type))
		    return False;
		a_arg = a_arg->next;
		b_arg = b_arg->next;
	    }
	    return True;
	}
	return False;
    case type_array:
	a_dim = TypeCountDimensions (a->array.dimensions);
	b_dim = TypeCountDimensions (b->array.dimensions);
	if (a_dim == 0 || b_dim == 0 || a_dim == b_dim)
	    return TypeIsCotype (a->array.type, b->array.type);
	return False;
    case type_hash:
	return (TypeIsCotype (a->hash.type, b->hash.type) &&
		TypeIsCotype (a->hash.keyType, b->hash.keyType));
    case type_struct:
    case type_union:
        a_st = a->structs.structs;
	b_st = b->structs.structs;
	for (n = 0; n < a_st->nelements; n++)
	{
	    Type	    *b_mem;

	    /* 
	     * Structs (or unions) are subtypes if they contain all
	     * of the a type members and those members are subtypes
	     */
	    b_mem = StructMemType (b_st, StructTypeAtoms(a_st)[n]);
	    if (!b_mem)
		return False;
	    if (!TypeIsCotype (BoxTypesElements(a_st->types)[n],
				  b_mem))
		return False;
	}
	return True;
    case type_name:
    case type_types:
	abort ();
    }
    return False;
}
#endif

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
	    return typePrim[rep_float];
    }
    else if (TypePoly (right))
    {
	if (TypeNumeric (left))
	    return typePrim[rep_float];
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
    else if (TypeNumeric (left) &&  TypeNumeric (right))
    {
	return typePrim[rep_integer];
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
	    return typePrim[rep_float];
    }
    else if (TypePoly (right))
    {
	if (TypeNumeric (left))
	    return typePrim[rep_float];
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
	if (TypeIsOrdered (aref->ref.ref, bref->ref.ref))
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
	return typePoly;
    if (ref->base.tag == type_ref)
	return ref->ref.ref;
    return 0;
}
		
static Type *
TypeUnaryGroup (Type *type)
{
    if (TypePoly (type))
	return typePrim[rep_float];
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
 * Type of an array or hash reference
 */
static Type *
TypeUnaryArray (Type *type)
{
    if (TypePoly (type))
	return typePoly;
    if (type->base.tag == type_array)
	return type->array.type;
    if (type->base.tag == type_hash)
	return type->hash.type;
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

/*
 * Return the least-upper bound for a boolean computation
 */
static Type *
TypeBinaryBool (Type *left, Type *right)
{
    if (TypePoly (left))
	left = typePrim[rep_bool];
    if (TypePoly (right))
	right = typePrim[rep_bool];
    if (TypeBool (left) && TypeBool (right))
	return left;
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
	RETURN (TypeCombineBinary (TypeNameType(left), tag, right));
    if (right->base.tag == type_name)
	RETURN (TypeCombineBinary (left, tag, TypeNameType(right)));
    
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
	if (TypeIsOrdered (left, right))
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
    case ASSIGNAND:
    case ASSIGNOR:
	if ((type = TypeBinaryBool (left, right)))
	    ret = TypeAdd (ret, type);
	break;
    case COLON:
	if (TypePoly (left) || TypePoly (right))
	    ret = TypeAdd (ret, typePoly);
	else if (TypeIsSupertype (left, right))
	    ret = TypeAdd (ret, left);
	else if (TypeIsSupertype (right, left))
	    ret = TypeAdd (ret, right);
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
	if (TypeIsOrdered (left, right))
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
	RETURN(TypeCombineUnary (TypeNameType(type), tag));
    
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
	RETURN(TypeCombineArray (TypeNameType(type), ndim, lvalue));
    
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
	else if (type->base.tag == type_hash)
	{
	    if (ndim == 1)
		ret = TypeAdd (ret, type->hash.type);
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
	return TypeCombineStruct (TypeNameType(type), tag, atom);
	
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
	return TypeCombineReturn (TypeNameType(type));

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
	return TypeCombineFunction (TypeNameType(type));

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
	return TypeCompatibleAssign (TypeNameType(a), b);
    case type_ref:
	if (ValueIsRef(b))
	{
	    if (RefValueGet (b))
		return TypeCompatibleAssign (a->ref.ref, RefValueGet (b));
	    else
		return TypeIsOrdered (a->ref.ref, RefType (b));
	}
	break;
    case type_func:
	if (ValueIsFunc(b))
	{
	    if (TypeIsOrdered (a->func.ret, b->func.code->base.type))
	    {
		ArgType *aarg = a->func.args, *barg = b->func.code->base.args;
    
		while (aarg || barg)
		{
		    if (!barg || !aarg)
			return False;
		    if (barg->varargs != aarg->varargs)
			return False;
		    if (!TypeIsOrdered (barg->type, aarg->type))
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

		    for (i = 0; i < ArrayNvalues(&b->array); i++)
		    {
			Value	v = ArrayValueGet (&b->array, i);
			if (v &&
			    !TypeCompatibleAssign (a->array.type, v))
			{
			    return False;
			}
		    }
		    return True;
		}
		else
		    return TypeIsOrdered (a->array.type, ArrayType(&b->array));
	    }
	}
	break;
    case type_hash:
	if (ValueIsHash (b))
	{
	    if (TypePoly (a->hash.type))
		return True;
	    if (TypePoly (b->hash.type))
	    {
		HashValue   h;
		Value	    *e = BoxElements (b->hash.elts);

		for (h = 0; h < b->hash.hashSet->size; h++)
		{
		    if (!TypeCompatibleAssign (a->hash.type,
					       HashEltValue(e)))
		    {
			return False;
		    }
		    if (!TypeCompatibleAssign (a->hash.keyType,
					       HashEltKey (e)))
		    {
			return False;
		    }
		    HashEltStep (e);
		}
		return True;
	    }
	    else
		return (TypeIsOrdered (a->hash.type, b->hash.type) &&
		        TypeIsOrdered (a->hash.keyType, b->hash.keyType));
	}
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
		if (!TypeIsOrdered (BoxTypesElements(st->types)[n], bt))
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

/*
 * Check to see if 'b' is a subtype of 'a'
 */

Bool
ValueIsType (Value b, TypePtr a)
{
    int	adim, bdim;
    int	n;
    
    if (!a || !b)
	return True;

    if (a->base.tag == type_types)
    {
	TypeElt	*elt;
	for (elt = a->types.elt; elt; elt = elt->next)
	    if (ValueIsType (b, elt->type))
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
	return ValueIsType (b, TypeNameType(a));
    case type_ref:
	if (ValueIsRef(b))
	{
	    if (RefValueGet (b))
		return ValueIsType (RefValueGet (b), a->ref.ref);
	    else
		return TypeIsSupertype (RefType(b), a->ref.ref);
	}
	break;
    case type_func:
	if (ValueIsFunc(b))
	{
	    if (TypeIsSupertype (b->func.code->base.type, a->func.ret))
	    {
		ArgType *aarg = a->func.args, *barg = b->func.code->base.args;
    
		while (aarg || barg)
		{
		    if (!barg || !aarg)
			return False;
		    if (barg->varargs != aarg->varargs)
			return False;
		    if (!TypeIsSupertype (aarg->type, barg->type))
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

		    for (i = 0; i < ArrayNvalues(&b->array); i++)
		    {
			Value	v = ArrayValueGet (&b->array, i);
			if (v &&
			    !ValueIsType (v, a->array.type))
			{
			    return False;
			}
		    }
		    return True;
		}
		else
		    return TypeIsSupertype (ArrayType(&b->array), a->array.type);
	    }
	}
	break;
    case type_hash:
	if (ValueIsHash (b))
	{
	    if (TypePoly (a->hash.type))
		return True;
	    if (TypePoly (b->hash.type))
	    {
		HashValue   h;
		Value	    *e = BoxElements (b->hash.elts);

		for (h = 0; h < b->hash.hashSet->size; h++)
		{
		    if (!ValueIsType (HashEltValue(e), a->hash.type))
		    {
			return False;
		    }
		    if (!ValueIsType (HashEltKey (e), a->hash.keyType))
		    {
			return False;
		    }
		    HashEltStep (e);
		}
		return True;
	    }
	    else
		return (TypeIsSupertype (b->hash.type, a->hash.type) &&
		        TypeIsSupertype (b->hash.keyType, a->hash.keyType));
	}
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
		if (!TypeIsSupertype (bt, BoxTypesElements(st->types)[n]))
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
	return TypeCanon (TypeNameType(type));
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
    typeRefPoly = NewTypeRef (typePoly, True);
    MemAddRoot (typeRefPoly);
    
    typeArrayInt = NewTypeArray (typePrim[rep_integer], 0, False);
    MemAddRoot (typeArrayInt);

    TypeCheckStack = StackCreate ();
    MemAddRoot (TypeCheckStack);
    TypeCheckLevel = 0;
    EXIT ();
    return 1;
}
