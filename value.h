/*
 * Copyright © 1988-2007 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * value.h
 *
 * type definitions for functions returning values
 */

#ifndef _VALUE_H_
#define _VALUE_H_
#include	<stdio.h>
#include	<stdarg.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<memory.h>
#include	<string.h>
#include	<signal.h>
#include	<assert.h>

typedef enum { False = 0, True = 1 }  	Bool;
typedef char		*Atom;
#ifndef MEM_TRACE
typedef const struct _valueType   ValueRep;
#else
typedef struct _valueType   ValueRep;
#endif
typedef struct _box	*BoxPtr;
typedef union _code	*CodePtr;
typedef struct _frame	*FramePtr;
typedef struct _thread	*ThreadPtr;
typedef struct _continuation	*ContinuationPtr;
typedef union _value	*Value;
typedef struct _obj	*ObjPtr;
typedef union _inst	*InstPtr;
typedef union _symbol	*SymbolPtr;

extern Atom AtomId (char *name);
#define AtomName(a) (a)
extern int  AtomInit (void);

typedef struct _AtomList    *AtomListPtr;
typedef union _type	    *TypePtr;
typedef struct _structType  *StructTypePtr;
typedef union _expr	    *ExprPtr;
typedef struct _catch	    *CatchPtr;
typedef struct _twixt	    *TwixtPtr;
typedef struct _jump	    *JumpPtr;

typedef struct _AtomList {
    DataType	*data;
    AtomListPtr	next;
    Atom	atom;
} AtomList;

AtomListPtr  NewAtomList (AtomListPtr next, Atom atom);

/*
 * computational radix for natural numbers.  Make sure the
 * definitions for digit, double_digit and signed_digit will
 * still work correctly.
 */

#if HAVE_STDINT_H

# include <stdint.h>

#define PtrToInt(p)	((int) (intptr_t) (p))
#define PtrToUInt(p)	((unsigned) (uintptr_t) (p))
#define IntToPtr(i)	((void *) (intptr_t) (i))
#define UIntToPtr(u)	((void *) (uintptr_t) (u))

# if HAVE_UINT64_T

/*
 * If stdint.h defines a 64 bit datatype, use 32 bit
 * chunks
 */

#  define DIGITBITS 32
typedef uint64_t    double_digit;
typedef int64_t	    signed_digit;
typedef uint32_t    digit;

# else

#  define DIGITBITS 16
typedef uint32_t    double_digit;
typedef uint16_t    digit;
typedef int32_t	    signed_digit;

# endif

#else

#define PtrToInt(p)	((int) (p))
#define PtrToUInt(p)	((unsigned) (p))
#define IntToPtr(i)	((void *) (i))
#define UIntToPtr(u)	((void *) (u))

# if SIZEOF_UNSIGNED_LONG_LONG == 8 || SIZEOF_UNSIGNED_LONG == 8
#  define DIGITBITS 32
# else
#  define DIGITBITS 16
# endif

# if DIGITBITS == 32

#  if SIZEOF_UNSIGNED_LONG_LONG == 8
typedef unsigned long long double_digit;
typedef long long signed_digit;
#  else
#   if SIZEOF_UNSIGNED_LONG == 8
typedef unsigned long double_digit;
typedef long signed_digit;
#   endif
#  endif

#  if SIZEOF_UNSIGNED_LONG == 4
typedef unsigned long digit;
#  else
#   if SIZEOF_UNSIGNED_INT == 4
typedef unsigned int digit;
#   endif
#  endif

# else

#  if SIZEOF_UNSIGNED_LONG == 4
typedef unsigned long double_digit;
typedef long signed_digit;
#  else
#   if SIZEOF_UNSIGNED_INT == 4
typedef unsigned int double_digit;
typedef int signed_digit;
#   endif
#  endif

#  if SIZEOF_UNSIGNED_INT == 2
typedef unsigned int digit;
#  else
#   if SIZEOF_UNSIGNED_SHORT == 2
typedef unsigned short digit;
#   endif
#  endif

# endif

#endif

#define MAXDIGIT	((digit) (BASE - 1))

#if DIGITBITS == 32
# define BASE		((double_digit) 65536 * (double_digit) 65536)
# define LBASE2	32
# define LLBASE2	5
#else
# define BASE		((double_digit) 65536)
# define LBASE2	16
# define LLBASE2	4
#endif

#define TwoDigits(n,i)	((double_digit) NaturalDigits(n)[i-1] | \
			 ((double_digit) NaturalDigits(n)[i] << LBASE2))
#define ModBase(t)  ((t) & (((double_digit) 1 << LBASE2) - 1))
#define DivBase(t)  ((t) >> LBASE2)

/* HashValues are stored in rep_int */

typedef int HashValue;

/*
 * Natural numbers form the basis for both the Integers and Rationals,
 * but needn't ever be exposed to the user
 */

typedef struct _natural {
    DataType	*type;
    int		length;
    digit	digits[0];
} Natural;

static inline int NaturalLength(Natural *n)
{
	return n->length;
}

static inline digit * NaturalDigits(Natural *n)
{
	return n->digits;
}

Natural	*NewNatural (unsigned value);
Natural *NewDoubleDigitNatural (double_digit dd);
Natural	*AllocNatural (int size);
Bool	NaturalEqual (Natural *, Natural *);
Bool	NaturalLess (Natural *, Natural *);
Natural	*NaturalPlus (Natural *, Natural *);
Natural *NaturalMinus (Natural *, Natural *);
Natural *NaturalTimes (Natural *, Natural *);
Natural *NaturalLand (Natural *, Natural *);
Natural *NaturalLor (Natural *, Natural *);
Natural	*NaturalCompliment (Natural *, int len);
Natural	*NaturalNegate (Natural *, int len);
Natural	*NaturalDivide (Natural *a, Natural *b, Natural **remp);
Natural	*NaturalGcd (Natural *a, Natural *b);
char	*NaturalSprint (char *, Natural *, int base, int *width);
Natural	*NaturalSqrt (Natural *);
Natural *NaturalFactor (Natural *n, Natural *max);
Natural *NaturalIntPow (Natural *n, int p);
Natural *NaturalPow (Natural *n, Natural *);
Natural *NaturalPowMod (Natural *n, Natural *p, Natural *m);
Natural	*NaturalRsl (Natural *v, int shift);
Natural	*NaturalLsl (Natural *v, int shift);
Natural	*NaturalMask (Natural *v, int bits);
int	NaturalPowerOfTwo (Natural *v);
int	NaturalEstimateLength (Natural *, int base);
void	NaturalCopy (Natural *, Natural *);
Bool	NaturalZero (Natural *);
Bool	NaturalEven (Natural *);
void	NaturalDigitMultiply (Natural *a, digit i, Natural *result);
digit	NaturalSubtractOffset (Natural *a, Natural *b, int offset);
digit	NaturalSubtractOffsetReverse (Natural *a, Natural *b, int offset);
Bool	NaturalGreaterEqualOffset (Natural *a, Natural *b, int offset);
void	NaturalAddOffset (Natural *a, Natural *b, int offset);
Natural *NaturalBdivmod (Natural *u_orig, Natural *v);
Natural *NaturalKaryReduction (Natural *u_orig, Natural *v);
int	NaturalWidth (Natural *u);
digit	DigitBmod (digit u, digit v, int s);
int	IntWidth (int i);
int	DoubleDigitWidth (double_digit i);
HashValue   NaturalHash (Natural *a);

extern Natural	*max_signed_digit_natural;
extern Natural	*max_int_natural;
extern Natural	*zero_natural;
extern Natural	*one_natural;
extern Natural	*two_natural;

typedef enum _sign { Positive = 0, Negative = 1 } Sign;

static inline Sign SignNegate(Sign sign) {
	return 1 - sign;
}

typedef enum _signcat {
	BothPositive = 0, FirstPositive = 1, SecondPositive = 2, BothNegative = 3
} Signcat;

static inline Signcat catagorize_signs(Sign s1, Sign s2) {
	return (s1 << 1) | s2;
}

typedef enum _binaryOp {
    PlusOp, MinusOp, TimesOp, DivideOp, DivOp, ModOp,
    LessOp, EqualOp, LandOp, LorOp, NumBinaryOp
} BinaryOp;

typedef enum _unaryOp {
    NegateOp, FloorOp, CeilOp, NumUnaryOp
} UnaryOp;

/*
 * Value representations.
 *
 * Values are represented by one of several data structures,
 * the first element of each value is a pointer back to a
 * data structure which contains the representation tag along
 * with functions that operate on the value
 */
typedef enum _rep {
	/* unknown type */
	rep_undef = -1,
	/* primitive types */
 	rep_int = 0,
	rep_integer = 1,
 	rep_rational = 2,
 	rep_float = 3,
 	rep_string = 4,
	rep_file = 5,
	rep_thread = 6,
	rep_semaphore = 7,
	rep_continuation = 8,
	rep_bool = 9,
	rep_foreign = 10,
	rep_void = 11,

	/* composite types */
	rep_ref = 12,
	rep_func = 13,

	/* mutable type */
 	rep_array = 14,
	rep_struct = 15,
	rep_union = 16,
	rep_hash = 17
} Rep;

/* because rep_undef is -1, using (unsigned) makes these a single compare */
#define Numericp(t)	((unsigned) (t) <= (unsigned) rep_float)
#define Integralp(t)	((unsigned) (t) <= (unsigned) rep_integer)


#define Mutablep(t)	((t) >= rep_array)

extern ValueRep    IntRep, IntegerRep, RationalRep, FloatRep;
extern ValueRep    StringRep, ArrayRep, FileRep;
extern ValueRep    RefRep, StructRep, UnionRep, HashRep;
extern ValueRep	   FuncRep, ThreadRep;
extern ValueRep    SemaphoreRep, ContinuationRep, UnitRep, BoolRep;
extern ValueRep    ForeignRep;

static inline Value NewInt(int i) {
	return (Value) IntToPtr ((((i) << 1) | 1));
}

static inline Sign IntSign(int i) {
	return (i) < 0 ? Negative : Positive;
}

/*
 * Use all but one bit to hold immediate integer values
 */
#define NICKLE_INT_BITS	    ((sizeof (int) * 8) - 1)
#define NICKLE_INT_SIGN	    (1U << (NICKLE_INT_BITS - 1))
/*
 * this bit holds any overflow; when different from SIGN,
 * an addition/subtraction has overflowed
 */
#define NICKLE_INT_CARRY    (1U << NICKLE_INT_BITS)
/*
 * An int fits in a 'nickle int' if the top two bits
 * are the same.  There are four initial values:
 *
 *  00 + 01 = 01
 *  01 + 01 = 10
 *  10 + 01 = 11
 *  11 + 01 = 00
 *
 * So, the two 'naughty' ones end up with the high bit set
 */
#define NICKLE_INT_CARRIED(r)	(((r) + NICKLE_INT_SIGN) & NICKLE_INT_CARRY)

#define MAX_NICKLE_INT	    ((int) ((unsigned) NICKLE_INT_SIGN - 1))
#define MIN_NICKLE_INT	    (-MAX_NICKLE_INT - 1)
#define MAX_NICKLE_SIGNED_DIGIT	((signed_digit) (((double_digit) 1 << (sizeof(signed_digit) * 8 - 1)) - 1))

#define One NewInt(1)
#define Zero NewInt(0)

static inline Bool ValueIsPtr (Value v) {
	return (PtrToInt(v) & 1) == 0;
}

static inline Bool ValueIsInt (Value v) {
	return !ValueIsPtr(v);
}

static inline int ValueInt(Value v) {
	return PtrToInt (v) >> 1;
}

static inline ValueRep *_ValueRep(Value v);

#define ValueRep(v) _ValueRep(v)
#define ValueIsInteger(v) (ValueRep(v) == &IntegerRep)
#define ValueIsRational(v) (ValueRep(v) == &RationalRep)
#define ValueIsFloat(v) (ValueRep(v) == &FloatRep)
#define ValueIsString(v) (ValueRep(v) == &StringRep)
#define ValueIsArray(v) (ValueRep(v) == &ArrayRep)
#define ValueIsFile(v) (ValueRep(v) == &FileRep)
#define ValueIsRef(v) (ValueRep(v) == &RefRep)
#define ValueIsStruct(v) (ValueRep(v) == &StructRep)
#define ValueIsUnion(v) (ValueRep(v) == &UnionRep)
#define ValueIsHash(v) (ValueRep(v) == &HashRep)
#define ValueIsFunc(v) (ValueRep(v) == &FuncRep)
#define ValueIsThread(v) (ValueRep(v) == &ThreadRep)
#define ValueIsSemaphore(v) (ValueRep(v) == &SemaphoreRep)
#define ValueIsContinuation(v) (ValueRep(v) == &ContinuationRep)
#define ValueIsUnit(v) (ValueRep(v) == &UnitRep)
#define ValueIsBool(v) (ValueRep(v) == &BoolRep)
#define ValueIsForeign(v) (ValueRep(v) == &ForeignRep)

/*
 * Aggregate types
 */
typedef struct _argType {
    DataType	*data;
    TypePtr	type;
    Bool	varargs;
    Atom	name;
    SymbolPtr	symbol;
    struct _argType *next;
} ArgType;

ArgType *NewArgType (TypePtr type, Bool varargs, Atom name,
		     SymbolPtr symbol, ArgType *next);

typedef enum _typeTag {
    type_prim, type_name, type_ref, type_func, type_array,
    type_struct, type_union, type_types, type_hash
} TypeTag;

typedef struct _typeBase {
    DataType	*data;
    TypeTag	tag;
} TypeBase;

typedef struct _typePrim {
    TypeBase	base;
    Rep	prim;
} TypePrim;

typedef struct _typeName {
    TypeBase	base;
    ExprPtr	expr;
    SymbolPtr	name;
} TypeName;

typedef struct _typeRef {
    TypeBase	base;
    TypePtr	ref;
    Bool	pointer;
} TypeRef;

typedef struct _typeFunc {
    TypeBase	base;
    TypePtr	ret;
    ArgType	*args;
} TypeFunc;

typedef enum _dimStorage {
    DimStorageNone, DimStorageGlobal, DimStorageStatic, DimStorageAuto
} DimStorage;

typedef struct _typeArray {
    TypeBase	base;
    TypePtr	type;
    ExprPtr	dimensions;
    int		dims;
    DimStorage	storage;
    Bool	resizable;
    union {
	BoxPtr	global;
	struct {
	    int	    element;
	    Bool    staticScope;
	    CodePtr code;
	} frame;
    } u;
} TypeArray;

typedef struct _typeHash {
    TypeBase	base;
    TypePtr	type;
    TypePtr	keyType;
} TypeHash;

typedef struct _typeStruct {
    TypeBase	    base;
    StructTypePtr   structs;
    Bool	    enumeration;
    TypePtr	    left, right;
} TypeStruct;

typedef struct _typeElt {
    DataType	    *data;
    struct _typeElt *next;
    union _type	    *type;
} TypeElt;

typedef struct _typeTypes {
    TypeBase	    base;
    TypeElt	    *elt;
} TypeTypes;

typedef union _type {
    TypeBase	base;
    TypePrim	prim;
    TypeName	name;
    TypeRef	ref;
    TypeFunc	func;
    TypeArray	array;
    TypeHash	hash;
    TypeStruct	structs;
    TypeTypes	types;
} Type;

typedef struct _argDecl {
    Type   *type;
    Atom    name;
} ArgDecl;

typedef struct _argList {
    ArgType *argType;
    Bool    varargs;
} ArgList;

extern Type	    *typePoly;
extern Type	    *typeRefPoly;
extern Type	    *typeFileError;
extern Type	    *typeArrayInt;
extern Type	    *typePrim[rep_void + 1];

Type	*NewTypeName (ExprPtr expr, SymbolPtr name);
Type	*NewTypeRef (Type *ref, Bool pointer);
Type	*NewTypePlus (Type *left, Type *right);
Type	*NewTypePointer (Type *ref);
Type	*NewTypeFunc (Type *ret, ArgType *args);
Type	*NewTypeArray (Type *type, ExprPtr dimensions, Bool resizable);
Type	*NewTypeHash (Type *type, Type *keyType);
Type	*NewTypeStruct (StructTypePtr structs);
Type	*NewTypeUnion (StructTypePtr structs, Bool enumeration);
Type	*NewTypeTypes (TypeElt *elt);
Type	*TypeCanon (Type *type);
void	TypeTypesAdd (Type *list, Type *type);
void	TypeTypesRemove (Type *list, Type *type);
Bool	TypeTypesMember (Type *list, Type *type);
int	TypeInit (void);
SymbolPtr   TypeNameName (Type *t);

Type	*TypeCombineBinary (Type *left, int tag, Type *right);
Type	*TypeCombineUnary (Type *down, int tag);
Type	*TypeCombineStruct (Type *type, int tag, Atom atom);
Type	*TypeCombineReturn (Type *type);
Type	*TypeCombineFunction (Type *type);
Type	*TypeCombineArray (Type *array, int ndim, Bool lvalue);
/* can assign value 'v' to variable of type 'dest' */
Bool	TypeCompatibleAssign (Type *dest, Value v);
/* is value 'v' a subtype of 't' */
Bool	ValueIsType (Value b, TypePtr a);
/* super is a supertype of sub */
Bool	TypeIsSupertype (Type *super, Type *sub);
/* a is a supertype of b or b is a supertype of a */
Bool	TypeIsOrdered (Type *a, Type *b);
/* a and b are 'cotypes' */
Bool	TypeIsCotype (Type *a, Type *b);

#define TypePoly(t)	((t)->base.tag == type_prim && (t)->prim.prim == rep_undef)
#define TypeBool(t)	((t)->base.tag == type_prim && (t)->prim.prim == rep_bool)
#define TypeString(t)	((t)->base.tag == type_prim && (t)->prim.prim == rep_string)

Bool	TypeNumeric (Type *t);
Bool	TypeIntegral (Type *t);
int	TypeCountDimensions (ExprPtr dims);

/*
 * storage classes
 */

typedef enum _class {
    class_global, class_static, class_arg, class_auto, class_const,
    class_typedef, class_namespace, class_exception, class_undef
} Class;

#define ClassLocal(c)	((c) == class_arg || (c) == class_auto)
#define ClassFrame(c)	((c) == class_static || ClassLocal(c))
#define ClassStorage(c)	((c) <= class_const)
#define ClassLvalue(c)	((c) <= class_auto)

typedef enum _publish {
    publish_private, publish_protected, publish_public, publish_extend
} Publish;

static inline Rep ValueTag(Value v);

typedef struct _baseValue {
    ValueRep	*type;
} BaseValue;

typedef struct _integer {
    BaseValue	base;
    Natural	*magn;
} Integer;

typedef struct _rational {
    BaseValue	base;
    Sign	sign;
    Natural	*num;
    Natural	*den;
} Rational;

typedef struct _fpart {
    DataType	*data;
    Natural	*mag;
    Sign	sign;
} Fpart;

typedef struct _float {
    BaseValue	base;
    Fpart	*mant;
    Fpart	*exp;
    unsigned	prec;
} Float;

typedef struct _string {
    BaseValue	    base;
    long	    length;
    char	    chars[0];
} String;

static inline char *
StringChars (String *s)
{
	return s->chars;
}

typedef struct _foreign {
    BaseValue	    base;
    const char	    *id;
    void	    *data;
    void	    (*mark)(void *);
    void	    (*free)(void *);
} Foreign;

/*
 * Resizable arrays are actually vectors of single entry
 * boxes.  Otherwise shrinking the array leaves old references
 * dangling.
 */

typedef struct _boxVector {
    DataType	*data;
    int		nvalues;
    TypePtr	type;
    BoxPtr	boxes[0];
} BoxVector, *BoxVectorPtr;

static inline BoxPtr *BoxVectorBoxes(BoxVector *v)
{
	return (BoxPtr *) v->boxes;
}

typedef struct _array {
    BaseValue	base;
    unsigned int	resizable : 1;
    unsigned int	ndim : (sizeof (int) * 8 - 1);
    union {
	BoxPtr		fix;
	BoxVectorPtr	resize;
    } u;
    int dims[0];
} Array;

typedef struct _io_chain {
    struct _io_chain	*next;
    int			size;
    int			used;
    int			ptr;
    unsigned char	buffer[0];
} FileChain, *FileChainPtr;

typedef struct _file {
    BaseValue	    base;
    union _value    *next;	    /* used to chain blocked files together */
    int		    fd;
    int		    pid;	    /* for pipes, process id */
    int		    status;	    /* from wait */
    int		    input_errno;    /* last input errno */
    int		    output_errno;   /* last output errno */
    int		    flags;
    int		    error;
    FileChainPtr    input;
    FileChainPtr    output;
    int		    sock_family;
} File;

#define FileBufferSize	4096
#define FileEOF		-1
#define FileBlocked	-2
#define FileError	-3
#define FileBuffer(ic)	((ic)->buffer)

#define FileReadable	    0x0001
#define FileWritable	    0x0002
#define FileOutputBlocked   0x0004
#define FileInputBlocked    0x0008
#define FileLineBuf	    0x0010
#define FileUnBuf	    0x0020
#define FileInputError      0x0040
#define FileOutputError	    0x0080
#define FileClosed	    0x0100
#define FileBlockWrites	    0x0200
#define FileEnd		    0x0400
#define FileString	    0x0800
#define FilePipe	    0x1000
#define FileIsPipe	    0x2000

typedef struct _boxTypes {
    DataType	*data;
    int		count;
    int		size;
    TypePtr	elements[0];
} BoxTypes, *BoxTypesPtr;

static inline TypePtr *BoxTypesElements(BoxTypes *bt) {
	return bt->elements;
}

static inline TypePtr BoxTypesValue(BoxTypes *bt, int e) {
	return BoxTypesElements(bt)[e];
}

static inline void BoxTypesValueSet(BoxTypes *bt, int e, TypePtr t) {
	BoxTypesElements(bt)[e] = t;
}

extern BoxTypesPtr  NewBoxTypes (int size);
extern int	    AddBoxType (BoxTypesPtr *btp, TypePtr t);

typedef struct _ref {
    BaseValue	base;
    BoxPtr	box;
    int		element;
} Ref;

typedef struct _structType {
    DataType	*data;
    int		nelements;
    BoxTypesPtr	types;
    Atom	atoms[0];
} StructType;

#define StructTypeAtoms(st)	((st)->atoms)

typedef struct _struct {
    BaseValue	base;
    StructType	*type;
    BoxPtr	values;
} Struct;

typedef struct _union {
    BaseValue	base;
    StructType	*type;
    Atom	tag;
    BoxPtr	value;
    Type	*types[0];
} Union;

typedef struct _func {
    BaseValue	base;
    CodePtr	code;
    FramePtr	staticLink;
    BoxPtr	statics;
} Func;

/*
 * This is a continuation, the same structure is also used within
 * threads, twixts and catches to hold an execution context
 */

typedef struct _continuation {
    union {
	BaseValue   value;
	DataType    *data;
    }		type;
    Value	value;	    /* accumulator */
    InstPtr	pc;	    /* program counter */
    ObjPtr	obj;	    /* reference to obj containing pc */
    FramePtr	frame;	    /* function call frame list */
    StackObject	*stack;	    /* value stack */
    CatchPtr	catches;    /* handled exceptions */
    TwixtPtr	twixts;	    /* pending twixts */
} Continuation;

typedef enum _ThreadState {
    ThreadRunning,
    ThreadSuspended,
    ThreadFinished
} ThreadState;

typedef struct _thread {
    /*
     * Execution continuation
     */
    Continuation    continuation;
    /*
     * Currently executing jump
     */
    JumpPtr	jump;
    /*
     * Thread status
     */
    ThreadState	state;
    int		priority;
    Value	sleep;
    int		id;
    int		partial;
    /*
     * Lower priority threads
     */
    Value	next;
} Thread;

#define PriorityMin	0
#define PriorityStart	100
#define PrioritySync	200
#define PriorityIo	300

typedef struct _semaphore {
    BaseValue	value;
    int		count;
    int		id;
} Semaphore;

/*
 * Set the continuation at dst to that at src.  Return the src
 * continuation instruction pointer
 */
InstPtr	ContinuationSet (ContinuationPtr    dst,
			 ContinuationPtr    src);

/*
 * Jump through a continuation, unwinding or rewinding appropriate twixt blocks
 */
Value
ContinuationJump (Value thread, ContinuationPtr src, Value ret, InstPtr *next);

/*
 * Mark memory referenced from a continuation,
 */
void	ContinuationMark (void *object);

/*
 * Initialize a continuation to default values
 */
void	ContinuationInit (ContinuationPtr continuation);

#ifdef DEBUG_JUMP
void	    ContinuationTrace (char *where, Continuation *continuation, int indent);
void	    ThreadCatches (Value thread);
#endif

/*
 * Hash tables.  Indexed by multiple typed values
 */

typedef const struct _HashSet {
    HashValue	entries;
    HashValue	size;
    HashValue	rehash;
} HashSetRec, *HashSetPtr;

/*
 * Hash elements are stored in boxes, with three elements
 * for each element (hash, key, value)
 *
 * Hash element states:
 *
 *  key	    value
 *  0	    0		    empty
 *  v	    0		    reference to uninitialized element
 *  0	    v		    deleted
 *  v	    v		    valid entry
 *
 *  So:
 *	key != 0		-> hash table includes
 *	value != 0		-> hash chain includes
 */

#define HashEltHash(e)	    ((e)[0])
#define HashEltKey(e)	    ((e)[1])
#define HashEltValue(e)	    ((e)[2])
#define HashEltSize	    3
#define HashEltStep(e)	    ((e) += HashEltSize)
#define HashEltCopy(d,s)    (((d)[0] = (s)[0]), \
			     ((d)[1] = (s)[1]), \
			     ((d)[2] = (s)[2]))
#define HashEltValid(e)	    (HashEltKey(e) != 0)
#define HashEltChained(e)   (HashEltValue(e) != 0)

typedef struct _hashTable {
    BaseValue	base;
    HashSetRec	*hashSet;
    HashValue	count;
    TypePtr	type;
    TypePtr	keyType;
    BoxPtr	elts;
    Value	def;
} HashTable, *HashTablePtr;

typedef union _value {
    BaseValue	value;
    Integer	integer;
    Rational	rational;
    Float	floats;
    String	string;
    Array	array;
    File	file;
    Ref		ref;
    Foreign	foreign;
    Struct	structs;
    Union	unions;
    Func	func;
    Thread	thread;
    Semaphore	semaphore;
    Continuation    continuation;
    HashTable	hash;
} ValueRec;

typedef Value	(*Binary) (Value, Value, int);

typedef Value	(*Unary) (Value, int);

typedef Value	(*Promote) (Value, Value);

typedef Value	(*Coerce) (Value);

typedef int	(*Hash) (Value);

#define DEFAULT_OUTPUT_PRECISION    -1
#define INFINITE_OUTPUT_PRECISION   -2

typedef	Bool	(*Output) (Value, Value, char format, int base, int width, int prec, int fill);

typedef ValueRep   *(*TypeCheck) (BinaryOp, Value, Value, int);

struct _valueType {
    DataType	data;
    Rep		tag;
    Binary	binary[NumBinaryOp];
    Unary	unary[NumUnaryOp];
    Promote	promote;
    Coerce	reduce;
    Output	print;
    TypeCheck	typecheck;
    Hash	hash;
};

static inline ValueRep *_ValueRep(Value v) {
	if (ValueIsInt(v))
		return &IntRep;
	return v->value.type;
}

static inline Rep ValueTag(Value v) {
	return ValueRep(v)->tag;
}

static inline Natural *IntegerMag(Value i) {
	return (Natural *) ((long) (i->integer.magn) & ~1);
}

static inline Sign IntegerSign(Value i) {
	return (Sign) ((long) (i->integer.magn) & 1);
}

typedef struct _boxReplace {
    DataType	    *data;
    BoxPtr	    new;
    int		    oldstride;
    int		    newstride;
} BoxReplace, *BoxReplacePtr;

typedef struct _box {
    DataType	    *data;
    unsigned long   constant : 1;
    unsigned long   homogeneous : 1;
    unsigned long   replace : 1;
    unsigned long   nvalues : (sizeof (unsigned long) * 8) - 3;
    union {
	BoxTypesPtr	    types;
	TypePtr		    type;
	BoxReplacePtr	    replace;
    } u;
    Value	    values[0];
} Box;

#if 1
#define BoxCheck(box)		assert (!(box)->replace)
#else
#define BoxCheck(box)
#endif

static inline Value *BoxElements(Box *box)
{
	BoxCheck(box);
	return box->values;
}

static inline Value BoxValueSet(Box *box, long e, Value v) {
	return BoxElements(box)[e] = v;
}

static inline Value BoxValueGet(Box *box, long e) {
	return BoxElements(box)[e];
}

static inline Bool BoxConstant(Box *box, int e) {
	return box->constant;
}

static inline Bool _BoxReplace(Box *box) {
	return box->replace;
}

#define BoxReplace(box) _BoxReplace(box)

static inline TypePtr _BoxType(Box *box, long e) {
	BoxCheck(box);
	if (box->homogeneous)
		return box->u.type;
	else
		return BoxTypesValue(box->u.types, e);
}

#define BoxType(box, e) _BoxType(box, e)

extern BoxPtr	NewBox (Bool constant, Bool array, int nvalues, TypePtr type);
extern BoxPtr	NewTypedBox (Bool array, BoxTypesPtr types);
void		BoxSetReplace (BoxPtr old, BoxPtr new, int oldstride, int newstride);
BoxPtr		BoxRewrite (BoxPtr box, int *ep);

typedef struct {
    DataType	*data;
    int		size;
    union {
	double	d;
	double_digit dd;
	void	*p;
    } values[0];
} DataCache, *DataCachePtr;

DataCachePtr	NewDataCache (int size);

static inline void *DataCacheValues(DataCache *vc) {
	return (void *) vc->values;
}

static inline int *ArrayDims (Array *a)
{
	return a->dims;
}

#define ArrayLimits(a)	    (ArrayDims(a) + (a)->ndim)
#define ArrayConstant

static inline long ArrayNvalues(Array *a) {
	if (a->resizable)
		return a->u.resize->nvalues;
	else
		return a->u.fix->nvalues;
}

static inline BoxPtr ArrayValueBox(Array *a, long i) {
	if (a->resizable)
		return BoxVectorBoxes(a->u.resize)[i];
	else
		return a->u.fix;
}

static inline int ArrayValueElt(Array *a, long i) {
	if (a->resizable)
		return 0;
	else
		return i;
}

static inline TypePtr ArrayType(Array *a) {
	if (a->resizable)
		return a->u.resize->type;
	else
		return a->u.fix->u.type;
}

#define ArrayValue(a,i)	    (BoxValue(ArrayValueBox(a,i),ArrayValueElt(a,i)))
#define ArrayValueGet(a,i)  (BoxValueGet(ArrayValueBox(a,i),ArrayValueElt(a,i)))

static inline void ArrayValueSet(Array *a, long i, Value v)
{
	BoxValueSet(ArrayValueBox(a,i),ArrayValueElt(a,i), v);
}

void
RefRewrite (Value r);

static inline void RefCheck(Value r) {
	if (BoxReplace(r->ref.box))
		RefRewrite(r);
}

static inline void RefValueSet(Value r, Value v) {
	RefCheck(r);
	BoxValueSet(r->ref.box, r->ref.element, v);
}

static inline Value RefValueGet(Value r) {
	RefCheck(r);
	return BoxValueGet(r->ref.box, r->ref.element);
}

static inline TypePtr RefType (Value r) {
	RefCheck(r);
	return BoxType(r->ref.box, r->ref.element);
}

static inline Bool RefConstant(Value r) {
	return BoxConstant(r->ref.box, r->ref.element);
}

Value	NewInteger (Sign sign, Natural *mag);
Value	NewIntInteger (int value);
Value	NewSignedDigitInteger (signed_digit d);
Value	NewRational (Sign sign, Natural *num, Natural *den);
Value	NewIntRational (int value);
Value	NewIntegerRational (Integer *);
Value	NewFloat (Fpart *mant, Fpart *exp, unsigned prec);
Value	NewIntFloat (int i, unsigned prec);
Value	NewIntegerFloat (Integer *i, unsigned prec);
Value	NewNaturalFloat (Sign sign, Natural *n, unsigned prec);
Value	NewRationalFloat (Rational *r, unsigned prec);
Value	NewValueFloat (Value av, unsigned prec);
Value	NewDoubleFloat (double d);
Value	NewContinuation (ContinuationPtr continuation, InstPtr pc);
Value	NewForeign (const char *id, void *data, void (*mark)(void *data), void (*free)(void *data));

unsigned    FpartLength (Fpart *a);

#define DEFAULT_FLOAT_PREC	256
#define REF_CACHE_SIZE		1031

extern DataCachePtr	refCache;

Value	NewString (long length);
Value	NewStrString (const char *);
Value	NewCharString (int c);
Value	NewArray (Bool constant, Bool resizable, TypePtr type, int ndim, int *dims);
void	ArrayResize (Value av, int dim, int size);
void	ArraySetDimensions (Value av, int *dims);
Value	NewHash (Bool constant, TypePtr keyType, TypePtr valueType);
Value	HashGet (Value hv, Value key);
void	HashSet (Value hv, Value key, Value value);
void	HashSetDef (Value hv, Value def);
Value	HashKeys (Value hv);
Value	HashRef (Value hv, Value key);
Value	HashTest (Value hv, Value key);
void	HashDelete (Value hv, Value key);
Value	HashCopy (Value hv);

Value	NewFile (int fd);
Value	NewRefReal (BoxPtr box, int element, Value *re);
char	*StringNextChar (char *src, unsigned *dst, long *length);
int	StringPutChar (unsigned c, char *dest);
int	StringLength (char *src, long length);
int	StringCharSize (unsigned c);
unsigned    StringGet (char *src, long len, int i);
char	*StrzPart (Value, char *error);

static inline Value
NewRef (BoxPtr box, int element)
{
    int	    c = (PtrToUInt (&BoxElements(box)[element])) % REF_CACHE_SIZE;
    Value   *re = (Value *) (DataCacheValues(refCache)) + c;
    Value   ret = *re;

    if (ret && ret->ref.box == box && ret->ref.element == element)
    {
	REFERENCE (ret);
	return ret;
    }
    return NewRefReal (box, element, re);
}

Value	NewStruct (StructType *type, Bool constant);
StructType  *NewStructType (int nelements);
Type	*BuildStructType (int nelements, ...);
Type	*StructMemType (StructType *st, Atom name);
Value	StructMemRef (Value sv, Atom name);
Value	StructMemValue (Value sv, Atom name);
Value	NewUnion (StructType *type, Bool constant);
Type	*BuildUnionType (int nelements, ...);
Type	*BuildEnumType (int nelements, ...);
Value	UnionValue (Value uv, Atom name);
Value	UnionRef (Value uv, Atom name);
Type	*BuildArrayType (Type *type, int ndim, ...);

Value	BinaryOperate (Value av, Value bv, BinaryOp operator);
Value	UnaryOperate (Value v, UnaryOp operator);
Value	NumericDiv (Value av, Value bv, int expandOk);
Value	NumericMod (Value av, Value bv, int expandOk);

# define	OK_TRUNC	1

extern Value	Blank, Elementless, Void, TrueVal, FalseVal;

# define True(v)	((v) == TrueVal)
# define False(v)	((v) != TrueVal)

Value	FileGetError (int err);
Value	FileGetErrorMessage (int err);
int	FileInput (Value);
int	FileOutput (Value, char);
void	FileUnput (Value, unsigned char);
int	FileInchar (Value);
int	FileOutchar (Value, int);
void	FileUnchar (Value, int);
Value   FileCreate (int fd, int flags);
int	FileFlush (Value, Bool block);
int	FileClose (Value);
Value	FileStringRead (char *string, int len);
Value	FileStringWrite (void);
Value	FileStringString (Value file);
void	FileSetFd (int fd), FileResetFd (int fd);
Bool	FileIsReadable (int fd);
Bool	FileIsWritable (int fd);
void	FilePutsc (Value, char *, long length);
void	FilePuts (Value, char *);
void	FilePutDoubleDigitBase (Value file, double_digit a, int base);
void	FilePutUIntBase (Value file, unsigned int a, int base);
void	FilePutIntBase (Value file, int a, int base);
void	FilePutInt (Value, int);
int	FileStringWidth (char *string, long length, char format);
void	FilePutString (Value f, char *string, long length, char format);
void	FilePutRep (Value f, Rep tag, Bool minimal);
void	FilePutClass (Value f, Class storage, Bool minimal);
void	FilePutPublish (Value f, Publish publish, Bool minimal);
void	FilePutType (Value f, Type *t, Bool minimal);
void	FilePutBaseType (Value f, Type *t, Bool minimal);
void	FilePutSubscriptType (Value f, Type *t, Bool minimal);
Value   FileFilter (char *program, char *args[], Value filev, int *errp);
Value	FileFopen (char *name, char *mode, int *errp);
Value	FileReopen (char *name, char *mode, Value file, int *errp);
Value   FileMakePipe (int *errp);
void	FilePutArgType (Value f, ArgType *at);
int	FileStatus (Value file);
void	FileCheckBlocked (Bool block);
void	FileSetBlocked (Value file, int flag);
void	FilePrintf (Value, char *, ...);
void	FileVPrintf (Value, char *, va_list);
void	FileSetBuffer (Value file, int buf);

extern Bool	anyFileWriteBlocked;
extern Bool	anyPipeReadBlocked;

extern BoxPtr	FileStdinBox, FileStdoutBox, FileStderrBox;

#define FileStdin	BoxValueGet(FileStdinBox, 0)
#define FileStdout	BoxValueGet(FileStdoutBox, 0)
#define FileStderr	BoxValueGet(FileStderrBox, 0)

typedef Value	(*BinaryFunc) (Value, Value);
typedef Value	(*UnaryFunc) (Value);

#define Plus(av,bv) BinaryOperate (av, bv, PlusOp)
#define Minus(av,bv) BinaryOperate (av, bv, MinusOp)
#define Times(av,bv) BinaryOperate (av, bv, TimesOp)
#define Divide(av,bv) BinaryOperate (av, bv, DivideOp)
#define Div(av,bv) BinaryOperate (av, bv, DivOp)
#define Mod(av,bv) BinaryOperate (av, bv, ModOp)
#define Less(av,bv) BinaryOperate (av, bv, LessOp)
#define Equal(av,bv) BinaryOperate (av, bv, EqualOp)
#define Land(av,bv) BinaryOperate (av, bv, LandOp)
#define Lor(av,bv) BinaryOperate (av, bv, LorOp)

int logbase2(int a);

Value	Greater (Value, Value), LessEqual (Value, Value);
Value	GreaterEqual (Value, Value), NotEqual (Value, Value);
Value	Not (Value);
Value	Negate (Value), Floor (Value), Ceil (Value);
Value	Truncate (Value);
Value	Round (Value);
Value	Pow (Value, Value), Factorial (Value), Reduce (Value);
Value	ShiftL (Value, Value), ShiftR (Value, Value);
Value	Gcd (Value, Value);
#undef GCD_DEBUG
#ifdef GCD_DEBUG
Value	Bdivmod (Value av, Value bv);
Value	KaryReduction (Value av, Value bv);
#endif
Value	Lxor(Value, Value), Lnot (Value);
Value   Popcount(Value);
Bool	Print (Value, Value, char format, int base, int width, int prec, int fill);
void	PrintError (char *s, ...);
HashValue HashCrc32 (unsigned char *bytes, int nbytes);
Value	CopyMutable (Value v);

static inline Value
Copy (Value v)
{
    if (v && Mutablep (ValueTag(v)))
	return CopyMutable (v);
    return v;
}

Value	ValueEqual (Value a, Value b, int expandOk);

Value	ValueHash (Value a);

/*
 * There are two kinds of signals:
 *
 *  aborting	    current instruction should be suspended
 *  non-aborting    current instruction should be completed
 *
 *  SIGIO and SIGALRM are non-aborting; otherwise computation would probably
 *  never make progress
 *
 *  SIGINTR is aborting
 *  All internal signals are aborting
 *
 *  An aborting signal marks 'aborting, signaling' and itself, this way
 *  low-level computations can check 'aborting' and the interpreter can
 *  check 'signaling' and then check the individual signals
 */

extern volatile Bool aborting;	/* abort current instruction */
extern volatile Bool signaling;	/* some signal is pending */

/*
 * Any signal state set by an signal handler must be volatile
 */

extern volatile Bool signalInterrupt;	/* keyboard interrupt */
extern volatile Bool signalTimer;	/* timer interrupt */
extern volatile Bool signalIo;		/* i/o interrupt */
extern volatile Bool signalProfile;	/* vtimer interrupt */
extern volatile Bool signalChild;	/* sub process interrupt */

#define SetSignalInterrupt()(aborting = signaling = signalInterrupt = True)
#define SetSignalTimer()    (signaling = signalTimer = True)
#define SetSignalIo()	    (signaling = signalIo = True)
#define SetSignalProfile()  (signaling = signalProfile = True)
#define SetSignalChild()    (signaling = signalChild = True)

/*
 * Any signal state set by regular code doesn't need to be volatile
 */

extern Bool signalSuspend;	/* current thread suspend */
extern Bool signalFinished;	/* current thread done */
extern Bool signalException;	/* current thread exception pending */
extern Bool signalError;	/* current thread run time error */

#define SetSignalSuspend()  (aborting = signaling = signalSuspend = True)
#define SetSignalFinished() (aborting = signaling = signalFinished = True)
#define SetSignalException()(aborting = signaling = signalException = True)
#define SetSignalError()    (aborting = signaling = signalError = True)

int
NaturalToInt (Natural *n);

double_digit
NaturalToDoubleDigit(Natural *n);

int	IntegerToInt (Integer *i);

int
IntegerFitsSignedDigit(Integer *i);

signed_digit
IntegerToSignedDigit(Integer *i);

int	IntPart (Value, char *error);

int	BoolPart (Value, char *error);

signed_digit
SignedDigitPart(Value v, char *error);

double	DoublePart (Value av, char *error);

Bool	Zerop (Value);
Bool	Negativep (Value);
Bool	Evenp (Value);

int	ArrayInit (void);
int	AtomInit (void);
int	FileInit (void);
int	IntInit (void);
int	HashInit (void);
int	NaturalInit (void);
int	IntegerInit (void);
int	RationalInit (void);
int	FpartInit (void);
int	StringInit (void);
int	StructInit (void);
int	RefInit (void);
int	ForeignInit (void);
int	ValueInit (void);

static inline Bool oneNp (Natural *n) {
	return n->length == 1 && NaturalDigits(n)[0] == 1;
}

static inline Bool zeroNp (Natural *n) {
	return n->length == 0;
}

void	ferr(int);
void	ignore_ferr (void);

#endif /* _VALUE_H_ */
