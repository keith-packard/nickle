/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
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
#include	"mem.h"
#include	"opcode.h"

typedef enum _Bool { False = 0, True = 1 }  	Bool;
typedef unsigned long	Atom;
typedef struct _valueType   ValueType;
typedef struct _box	*BoxPtr;
typedef union _code	*CodePtr;
typedef struct _frame	*FramePtr;
typedef struct _thread	*ThreadPtr;
typedef struct _continuation	*ContinuationPtr;
typedef union _value	*Value;
typedef struct _obj	*ObjPtr;
typedef union _inst	*InstPtr;


extern Atom AtomId (char *name);
extern char *AtomName (Atom id);
extern int  AtomInit (void);

typedef struct _AtomList    *AtomListPtr;
typedef union _types	    *TypesPtr;
typedef struct _structType  *StructTypePtr;
typedef union _expr	    *ExprPtr;
typedef struct _catch	    *CatchPtr;
typedef struct _twixt	    *TwixtPtr;
typedef struct _jump	    *JumpPtr;

typedef struct _AtomList {
    DataType	*data;
    AtomListPtr	next;
    Atom	id;
} AtomList;

/*
 * computational radix for natural numbers.  Make sure the
 * definitions for digit, double_digit and signed_digit will
 * still work correctly.
 */

#if SIZEOF_LONG_LONG == 8
# define BASE		((double_digit) 65536 * (double_digit) 65536)
# define LBASE2	32
# define LLBASE2	5
# define DIGITBITS	32
#else
# define BASE		((double_digit) 65536)
# define LBASE2	16
# define LLBASE2	4
# define DIGITBITS	16
#endif

#define MAXDIGIT	((digit) (BASE - 1))

/*
 * Natural numbers form the basis for both the Integers and Rationals,
 * but needn't ever be exposed to the user
 */
#if DIGITBITS <= 16
typedef unsigned short	digit;		/* must hold 0 .. BASE - 1 */
typedef unsigned int	double_digit;	/* must hold 0 .. (BASE - 1) * (BASE - 1) + BASE-2 */
typedef int		signed_digit;	/* must hold -BASE .. BASE - 1 */
#else
typedef unsigned int	    digit;
typedef unsigned long long  double_digit;
typedef signed long long    signed_digit;
#endif

#define TwoDigits(n,i)	((double_digit) NaturalDigits(n)[i-1] | \
			 ((double_digit) NaturalDigits(n)[i] << LBASE2))
#define ModBase(t)  ((t) & (((double_digit) 1 << LBASE2) - 1))
#define DivBase(t)  ((t) >> LBASE2)
    
typedef struct _natural {
    DataType	*type;
    int		length;
} Natural;

#define NaturalLength(n)    ((n)->length)
#define NaturalDigits(n)    ((digit *) ((n) + 1))

Natural	*NewNatural (unsigned value);
Natural	*AllocNatural (int size);
int	NaturalEqual (Natural *, Natural *);
int	NaturalLess (Natural *, Natural *);
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

extern Natural	*max_int_natural;
extern Natural	*zero_natural;
extern Natural	*one_natural;
extern Natural	*two_natural;

typedef enum _sign { Positive, Negative } Sign;

#define SignNegate(sign)    ((sign) == Positive ? Negative : Positive)

typedef enum _signcat {
	BothPositive, FirstPositive, SecondPositive, BothNegative
} Signcat;

# define catagorize_signs(s1,s2)\
	((s1) == Positive ? \
		((s2) == Positive ? BothPositive : FirstPositive) \
	: \
		((s2) == Positive ? SecondPositive : BothNegative))

typedef enum _binaryOp {
    PlusOp, MinusOp, TimesOp, DivideOp, DivOp, ModOp,
    LessOp, EqualOp, LandOp, LorOp, NumBinaryOp
} BinaryOp;

typedef enum _unaryOp {
    NegateOp, FloorOp, CeilOp, NumUnaryOp
} UnaryOp;

typedef enum _type {
	type_undef = -1,
 	type_int = 0,
	type_integer = 1,
 	type_rational = 2,
 	type_float = 3,
 	type_string = 4,
	type_file = 5,
	type_thread = 6,
	type_semaphore = 7,
	type_continuation = 8,
    
 	type_array = 9,
	type_ref = 10,
	type_struct = 11,
	type_union = 12,
	type_func = 13
} Type;

/*
 * Aggregate types
 */
typedef struct _argType {
    DataType	*data;
    TypesPtr	type;
    Bool	varargs;
    Atom	name;
    struct _argType *next;
} ArgType;

ArgType *NewArgType (TypesPtr type, Bool varargs, Atom name, ArgType *next);

typedef enum _typesTag {
    types_prim, types_name, types_ref, types_func, types_array, 
    types_struct, types_union, types_unit,
} TypesTag;
    
typedef struct _typesBase {
    DataType	*data;
    TypesTag	tag;
} TypesBase;

typedef struct _typesPrim {
    TypesBase	base;
    Type	prim;
} TypesPrim;

typedef struct _typesName {
    TypesBase	base;
    Atom	name;
    TypesPtr	type;
} TypesName;

typedef struct _typesRef {
    TypesBase	base;
    TypesPtr	ref;
} TypesRef;

typedef struct _typesFunc {
    TypesBase	base;
    TypesPtr	ret;
    ArgType	*args;
} TypesFunc;

typedef struct _typesArray {
    TypesBase	base;
    TypesPtr	type;
    ExprPtr	dimensions;
} TypesArray;

typedef struct _typesStruct {
    TypesBase	    base;
    StructTypePtr   structs;
} TypesStruct;    

typedef union _types {
    TypesBase	base;
    TypesPrim	prim;
    TypesName	name;
    TypesRef	ref;
    TypesFunc	func;
    TypesArray	array;
    TypesStruct	structs;
} Types;

typedef struct _argDecl {
    Types   *type;
    Atom    name;
} ArgDecl;

typedef struct _argList {
    ArgType *argType;
    Bool    varargs;
} ArgList;

extern Types	    *typesPoly;
extern Types	    *typesUnit;
extern Types	    *typesGroup;
extern Types	    *typesField;
extern Types	    *typesRefPoly;
extern Types	    *typesNil;
extern Types	    *typesPolyUnit;
extern Types	    *typesPrim[type_continuation - type_int + 1];

Types	*NewTypesPrim (Type prim);
Types	*NewTypesName (Atom name, Types *type);
Types	*NewTypesRef (Types *ref);
Types	*NewTypesFunc (Types *ret, ArgType *args);
Types	*NewTypesArray (Types *type, ExprPtr dimensions);
Types	*NewTypesStruct (StructTypePtr structs);
Types	*NewTypesUnion (StructTypePtr structs);
Types	*NewTypesUnit (void);
Types	*TypesCanon (Types *type);
Type	BaseType (Types *type);
int	TypesInit (void);

#define TypesUnionElements(t) ((Types **) (&t->unions + 1))

Types	*TypeCombineBinary (Types *left, int tag, Types *right);
Types	*TypeCombineUnary (Types *down, int tag);
Types	*TypeCombineStruct (Types *type, int tag, Atom atom);
Types	*TypeCombineReturn (Types *type);
Types	*TypeCombineFunction (Types *type);
Types	*TypeCombineArray (Types *array, int ndim, Bool lvalue);
Bool	TypeCompatibleAssign (Types *dest, Value v, Bool shallow);
/* Bool	TypeEqual (Types *a, Types *b); unused */
Bool	TypeCompatible (Types *a, Types *b, Bool contains);
Bool	TypePoly (Types *t);
Bool	TypeNumeric (Types *t);
Bool	TypeIntegral (Types *t);
Bool	TypeString (Types *t);
int	TypeCountDimensions (ExprPtr dims);

/*
 * storage classes
 */

typedef enum _class {
    class_global, class_static, class_arg, class_auto, 
    class_typedef, class_namespace, class_exception, class_undef,
} Class;

#define ClassLocal(c)	((c) == class_arg || (c) == class_auto)
#define ClassFrame(c)	((c) == class_static || ClassLocal(c))
#define ClassStorage(c)	((c) <= class_auto)

typedef enum _publish {
    publish_public, publish_private, publish_extend
} Publish;

typedef struct _baseValue {
    ValueType	*type;
    Type	tag;
} BaseValue;

typedef struct _int {
    BaseValue	base;
    int		value;
} Int;

typedef struct _integer {
    BaseValue	base;
    Sign	sign;
    Natural	*mag;
} Integer;

typedef struct _rational {
    BaseValue	base;
    Sign	sign;
    Natural	*num;
    Natural	*den;
} Rational;

typedef struct _fpart {
    DataType	data;
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
    BaseValue	base;
} String;

#define StringChars(s)	    ((char *) ((s) + 1))

typedef struct _array {
    BaseValue	base;
    Types	*type;
    int		ndim;
    int		ents;
    int		*dim;
    BoxPtr	values;
} Array;

typedef struct _io_chain {
    DataType		*data;
    struct _io_chain	*next;
    int			size;
    int			used;
    int			ptr;
} FileChain, *FileChainPtr;

typedef struct _file {
    BaseValue	    base;
    union _value    *next;	/* used to chain blocked files together */
    int		    fd;
    int		    pid;	/* for pipes, process id */
    int		    status;	/* from wait */
    int		    flags;
    int		    error;
    FileChainPtr    input;
    FileChainPtr    output;
} File;

#define FileBufferSize	4096
#define FileEOF		-1
#define FileBlocked	-2
#define FileError	-3
#define FileBuffer(ic)	((unsigned char *) ((ic) + 1))

#define FileOutputBlocked   0x01
#define FileInputBlocked    0x02
#define FileLineBuf	    0x04
#define FileUnBuf	    0x08
#define FileInputError      0x10
#define FileOutputError	    0x20
#define FileClosed	    0x40
#define FileBlockWrites	    0x80
#define FileEnd		    0x100
#define FileString	    0x200
#define FilePipe	    0x400

typedef struct _ref {
    BaseValue	base;
    BoxPtr	box;
    int		element;
} Ref;

#define RefValueSet(r,v) BoxValueSet((r)->ref.box, (r)->ref.element, (v))
#define RefValue(r)	BoxValue((r)->ref.box, (r)->ref.element)
#define RefValueGet(r)	BoxValueGet((r)->ref.box, (r)->ref.element)
#define RefType(r)	BoxType((r)->ref.box, (r)->ref.element)
#define RefConstant(r)    BoxConstant((r)->ref.box)

typedef struct _structElement {
    TypesPtr	type;
    Atom	name;
} StructElement;

typedef struct _structType {
    DataType	data;
    int		nelements;
} StructType;

#define StructTypeElements(st)	((StructElement *) (st + 1))

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
} Union;

typedef struct _func {
    BaseValue	base;
    CodePtr	code;
    FramePtr	staticLink;
    BoxPtr	statics;
    ObjPtr	staticInit;
} Func;

typedef enum _ThreadState {
    ThreadRunning = 0,
    ThreadSuspended = 1,
    ThreadInterrupted = 2,
    ThreadFinished = 4,
} ThreadState;

typedef struct _thread {
    BaseValue	value;
    /*
     * Execution context
     */
    Value	v;
    StackObject	*stack;
    InstPtr	pc;
    FramePtr	frame;
    ObjPtr	code;
    CatchPtr	catches;
    TwixtPtr	twixts;
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

typedef struct _mutex {
    BaseValue	value;
    Value	owner;
} Mutex;

typedef struct _semaphore {
    BaseValue	value;
    int		count;
    int		id;
} Semaphore;

typedef struct _continuation {
    BaseValue	value;
    FramePtr	frame;
    InstPtr	pc;
    StackObject	*stack;
    CatchPtr	catches;
    TwixtPtr	twixts;
} Continuation;

typedef union _value {
    BaseValue	value;
    Int		ints;
    Integer	integer;
    Rational	rational;
    Float	floats;
    String	string;
    Array	array;
    File	file;
    Ref		ref;
    Struct	structs;
    Union	unions;
    Func	func;
    Thread	thread;
    Mutex	mutex;
    Semaphore	semaphore;
    Continuation    continuation;
} ValueRec;

typedef Value	(*Binary) (Value, Value, int);

typedef Value	(*Unary) (Value, int);

typedef Value	(*Promote) (Value, Value);

typedef Value	(*Coerce) (Value);

#define DEFAULT_OUTPUT_PRECISION    -1
#define INFINITE_OUTPUT_PRECISION   -2

typedef	Bool	(*Output) (Value, Value, char format, int base, int width, int prec, unsigned char fill);

typedef ValueType   *(*TypeCheck) (BinaryOp, Value, Value, int);

struct _valueType {
    DataType	data;
    Binary	binary[NumBinaryOp];
    Unary	unary[NumUnaryOp];
    Promote	promote;
    Coerce	reduce;
    Output	print;
    TypeCheck	typecheck;
};

typedef struct _boxElement {
    Value	value;
    Types	*type;
} BoxElement;

typedef struct _box {
    DataType	*data;
    Bool	constant;
    int		nvalues;
} Box;

#define BoxElements(box)	((BoxElement *) ((box) + 1))
#define BoxValueSet(box,e,v)	((BoxElements(box)[e].value) = (v))
#define BoxValueGet(box,e)	((BoxElements(box)[e].value))
#define BoxConstant(box)	((box)->constant)
#define BoxType(box,e)		(BoxElements(box)[e].type)

extern BoxPtr	NewBox (Bool constant, int nvalues);
extern Value	BoxValue (BoxPtr box, int e);

typedef struct _boxTypes {
    DataType	*data;
    int		count;
    int		size;
} BoxTypes, *BoxTypesPtr;

#define BoxTypesElements(bt)	((TypesPtr *) ((bt) + 1))
#define BoxTypesValue(bt,e)	(BoxTypesElements(bt)[e])

extern BoxTypesPtr NewBoxTypes (int size);

extern int	AddBoxTypes (BoxTypesPtr *btp, TypesPtr t);

extern BoxPtr	NewTypedBox (Bool constant, BoxTypesPtr types);
			     
Value	NewVoid (void);
Value	NewInt (int value);
Value	NewInteger (Sign sign, Natural *mag);
Value	NewIntInteger (int value);
Value	NewRational (Sign sign, Natural *num, Natural *den);
Value	NewIntRational (int value);
Value	NewIntegerRational (Integer *);
Value	NewFloat (Fpart *mant, Fpart *exp, unsigned prec);
Value	NewIntFloat (int i, unsigned prec);
Value	NewIntegerFloat (Integer *i, unsigned prec);
Value	NewNaturalFloat (Sign sign, Natural *n, unsigned prec);
Value	NewRationalFloat (Rational *r, unsigned prec);
Value	NewValueFloat (Value av, unsigned prec);
Value	NewContinuation (FramePtr frame, InstPtr pc, 
			 StackObject *stack,
			 CatchPtr catches,
			 TwixtPtr twixts);

unsigned    FpartLength (Fpart *a);

#define DEFAULT_FLOAT_PREC	256

Value	NewString (int);
Value	NewStrString (char *);
Value	NewArray (Bool constant, TypesPtr type, int ndim, int *dims);
Value	NewFile (int fd);
Value	NewRef (BoxPtr box, int element);
Value	NewStruct (StructType *type, Bool constant);
StructType  *NewStructType (int nelements);
Types	*StructTypes (StructType *st, Atom name);
Value	StructRef (Value sv, Atom name);
Value	StructValue (Value sv, Atom name);
Value	NewUnion (StructType *type, Bool constant);
Value	UnionValue (Value uv, Atom name);
Value	UnionRef (Value uv, Atom name);

Value	BinaryOperate (Value av, Value bv, BinaryOp operator);
Value	UnaryOperate (Value v, UnaryOp operator);
Value	NumericDiv (Value av, Value bv, int expandOk);

# define	OK_TRUNC	1

extern Value	Zero, One, Blank, Empty, Elementless, Void;

# define True(v)	(!Zerop(v))
# define False(v)	(Zerop(v))

int	FileInput (Value);
void	FileOutput (Value, char);
void	FileUnput (Value, unsigned char);
Value   FileCreate (int fd);
int	FileFlush (Value);
int	FileClose (Value);
Value	FileStringRead (char *string, int len);
Value	FileStringWrite (void);
Value	FileStringString (Value file);
void	FileSetFd (int fd), FileResetFd (int fd);
void	FilePuts (Value, char *);
void	FilePutUIntBase (Value file, unsigned int a, int base);
void	FilePutIntBase (Value file, int a, int base);
void	FilePutInt (Value, int);
void	FilePutType (Value f, Type tag, Bool minimal);
void	FilePutClass (Value f, Class storage, Bool minimal);
void	FilePutPublish (Value f, Publish publish, Bool minimal);
void	FilePutTypes (Value f, Types *t, Bool minimal);
Value	FileFopen (char *name, char *mode);
void	FilePutArgTypes (Value f, ArgType *at);
Value	FilePopen (char *program, char *args[], char *mode);
int	FileStatus (Value file);
void	FileCheckBlocked (void);
void	FileSetBlocked (Value file, int flag);
void	FilePrintf (Value, char *, ...);
void	FileVPrintf (Value, char *, va_list);
void	FileSetBuffer (Value file, int buf);

extern Bool	anyFileWriteBlocked;
extern Bool	anyFileReadBlocked;

extern Value    FileStdin, FileStdout, FileStderr;

Value	Plus (Value, Value), Minus (Value, Value);
Value	Times (Value, Value), Divide (Value, Value), Div (Value, Value);
Value	Mod (Value, Value);
Value	Equal (Value, Value), Less (Value, Value);
Value	Greater (Value, Value), LessEqual (Value, Value);
Value	GreaterEqual (Value, Value), NotEqual (Value, Value);
Value	Not (Value);
Value	Negate (Value), Floor (Value), Ceil (Value);
Value	Truncate (Value);
Value	Round (Value);
Value	Pow (Value, Value), Factorial (Value), Reduce (Value);
Value	ShiftL (Value, Value), ShiftR (Value, Value);
Value	Gcd (Value, Value);
Value	Lxor(Value, Value), Land (Value, Value);
Value	Lor (Value, Value), Lnot (Value);
Bool	Print (Value, Value, char format, int base, int width, int prec, unsigned char fill);
void	RaiseError (char *s, ...);
void	PrintError (char *s, ...);
Value	Copy (Value);
Value	Default (TypesPtr);
Value	ValueEqual (Value a, Value b, int expandOk);

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

#define SetSignalInterrupt()(aborting = signaling = signalInterrupt = True)
#define SetSignalTimer()    (signaling = signalTimer = True)
#define SetSignalIo()	    (signaling = signalIo = True)

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

int	NaturalToInt (Natural *);
int	IntegerToInt (Integer *);
int	IntPart (Value, char *error);

Bool	Numericp (Type);
Bool	Integralp (Type);
Bool	Zerop (Value);
Bool	Negativep (Value);
Bool	Evenp (Value);

int	ArrayInit (void);
int	AtomInit (void);
int	FileInit (void);
int	IntInit (void);
int	NaturalInit (void);
int	RationalInit (void);
int	FpartInit (void);
int	StringInit (void);
int	StructInit (void);
int	ValueInit (void);

#ifndef MAXINT
# define MAXINT	((int) (((unsigned) (~0)) >> 1))
#endif
#ifndef MININT
# define MININT (-MAXINT - 1)
#endif

# define oneNp(n)	((n)->length == 1 && NaturalDigits(n)[0] == 1)
# define zeroNp(n)	((n)->length == 0)

void	ferr(int);
void	ignore_ferr (void);

#endif /* _VALUE_H_ */
