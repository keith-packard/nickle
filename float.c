/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<math.h>
#include	"nickle.h"

Fpart	*zero_fpart, *one_fpart;

#if 0
#define DebugV(s,v) { \
    FilePuts (FileStdout, s); \
    FilePuts (FileStdout, " "); \
    Print (FileStdout, v, 'g', 0, 0, DEFAULT_OUTPUT_PRECISION, ' '); \
    FilePuts (FileStdout, "\n"); \
}

#define DebugN(s,n) { \
    int	print_width; \
    FilePuts (FileStdout, s); \
    FilePuts (FileStdout, " "); \
    FilePuts (FileStdout, NaturalSprint (0, n, 10, &print_width)); \
    FilePuts (FileStdout, "\n"); \
}

#define DebugFp(s,f) { \
    int	print_width; \
    FilePuts (FileStdout, s); \
    FilePuts (FileStdout, " "); \
    if (f->sign == Negative) FilePuts (FileStdout, "-"); \
    FilePuts (FileStdout, NaturalSprint (0, (f)->mag, 10, &print_width)); \
    FilePuts (FileStdout, "\n"); \
}
#define DebugF(s,f) { \
    DebugFp(s,(f)->mant); \
    DebugFp(" e ", (f)->exp); \
}
#else
#define DebugV(s,v)
#define DebugN(s,n)
#define DebugFp(s,f)
#define DebugF(s,f)
#endif

static void
FpartMark (void *object)
{
    Fpart   *f = object;
    MemReference (f->mag);
}

DataType    FpartType = { FpartMark, 0 };

static Fpart *
NewFpart (Sign sign, Natural *mag)
{
    ENTER ();
    Fpart   *ret;

    if (NaturalZero (mag))
	sign = Positive;
    ret = ALLOCATE (&FpartType, sizeof (Fpart));
    ret->sign = sign;
    ret->mag = mag;
    RETURN (ret);
}

static Fpart *
NewIntFpart (int i)
{
    ENTER ();
    Sign	    sign;
    unsigned long   mag;

    if (i < 0)
    {
	sign = Negative;
	mag = -i;
    }
    else
    {
	sign = Positive;
	mag = i;
    }
    RETURN (NewFpart (sign, NewNatural (mag)));
}

static Fpart *
NewValueFpart (Value v)
{
    ENTER ();
    Fpart   *ret;
    switch (v->value.tag) {
    case type_int:
	ret = NewIntFpart (v->ints.value);
	break;
    case type_integer:
	ret = NewFpart (v->integer.sign, v->integer.mag);
	break;
    default:
	ret = zero_fpart;
	break;
    }
    RETURN (ret);
}

static Fpart *
FpartAdd (Fpart *a, Fpart *b, Bool negate)
{
    ENTER ();
    Fpart   *ret;
    
    switch (catagorize_signs(a->sign, negate ? SignNegate (b->sign):b->sign)) {
    default:
    case BothPositive:
	ret = NewFpart (Positive, NaturalPlus (a->mag, b->mag));
	break;
    case FirstPositive:
	if (NaturalLess (a->mag, b->mag))
	    ret = NewFpart (Negative, NaturalMinus (b->mag, a->mag));
	else
	    ret =  NewFpart (Positive, NaturalMinus (a->mag, b->mag));
	break;
    case SecondPositive:
	if (NaturalLess (a->mag, b->mag))
	    ret =  NewFpart (Positive, NaturalMinus (b->mag, a->mag));
	else
	    ret =  NewFpart (Negative, NaturalMinus (a->mag, b->mag));
	break;
    case BothNegative:
	ret =  NewFpart (Negative, NaturalPlus (a->mag, b->mag));
	break;
    }
    RETURN (ret);
}

static Fpart *
FpartMult (Fpart *a, Fpart *b)
{
    ENTER ();
    Sign    sign;

    sign = Positive;
    if (a->sign != b->sign)
	sign = Negative;
    RETURN (NewFpart (sign, NaturalTimes (a->mag, b->mag)));
}

static Fpart *
FpartDivide (Fpart *a, Fpart *b)
{
    ENTER ();
    Natural *rem;
    Natural *quo;
    Sign    sign;

    sign = Positive;
    if (a->sign != b->sign)
	sign = Negative;
    
    quo = NaturalDivide (a->mag, b->mag, &rem);
    RETURN (NewFpart (sign, quo));
}

static Fpart *
FpartRsl (Fpart *a, int shift)
{
    ENTER ();
    RETURN (NewFpart (a->sign, NaturalRsl (a->mag, shift)));
}

static Fpart *
FpartLsl (Fpart *a, int shift)
{
    ENTER ();
    RETURN (NewFpart (a->sign, NaturalLsl (a->mag, shift)));
}

static Bool
FpartLess (Fpart *a, Fpart *b)
{
    switch (catagorize_signs(a->sign, b->sign)) {
    default:
    case BothPositive:
	return NaturalLess (a->mag, b->mag);
    case FirstPositive:
	return False;
    case SecondPositive:
	return True;
    case BothNegative:
	return NaturalLess (b->mag, a->mag);
    }
}

static Bool
FpartEqual (Fpart *a, Fpart *b)
{
    switch (catagorize_signs(a->sign, b->sign)) {
    default:
    case BothPositive:
    case BothNegative:
	return NaturalEqual (a->mag, b->mag);
    case FirstPositive:
	return False;
    case SecondPositive:
	return False;
    }
}

static Bool
FpartZero (Fpart *a)
{
    return NaturalZero (a->mag);
}

unsigned
FpartLength (Fpart *a)
{
    unsigned	bits;
    digit	top;

    if (a->mag->length == 0)
	return 0;
    
    bits = (a->mag->length - 1) * LBASE2;
    top = NaturalDigits(a->mag)[a->mag->length - 1];
    while (top)
    {
	bits++;
	top >>= 1;
    }
    return bits;
}

static unsigned
FpartZeros (Fpart *a)
{
    int	    i;
    int	    zeros = 0;
    digit   top;

    if (a->mag->length == 0)
	return 0;
    for (i = 0; i < a->mag->length - 1; i++)
    {
	if (NaturalDigits(a->mag)[i] != 0)
	    break;
	zeros += LBASE2;
    }
    top = NaturalDigits(a->mag)[i];
    while ((top & 1) == 0)
    {
	zeros++;
	top >>= 1;
    }
    return zeros;
}

static Fpart *
FpartNegate (Fpart *a)
{
    ENTER ();
    RETURN (NewFpart (SignNegate (a->sign), a->mag));
}

int
FpartInit (void)
{
    ENTER ();
    zero_fpart = NewFpart (Positive, zero_natural);
    MemAddRoot (zero_fpart);
    one_fpart = NewFpart (Positive, one_natural);
    MemAddRoot (one_fpart);
    EXIT ();
    return 1;
}

static Value
FloatAdd (Value av, Value bv, int expandOk, Bool negate)
{
    ENTER ();
    Value	ret;
    Float	*a = &av->floats, *b = &bv->floats;
    Fpart	*dist;
    Fpart	*amant, *bmant, *mant;
    Fpart	*exp;
    int		d;
    unsigned	prec;
    int		alen, blen;

    dist = FpartAdd (a->exp, b->exp, True);
    ret = 0;
    if (NaturalLess (dist->mag, max_int_natural))
    {
	d = NaturalToInt (dist->mag);
	if (dist->sign == Negative)
	    d = -d;
	
	amant = a->mant;
	bmant = b->mant;
	alen = FpartLength (amant);
	blen = FpartLength (bmant);
	prec = 0;
	exp = 0;
	if (d >= 0)
	{
	    if (alen + d <= blen + a->prec)
	    {
		amant = FpartLsl (amant, d);
		exp = b->exp;
		prec = b->prec;
		if (a->prec + d < prec)
		    prec = a->prec + d;
	    }
	}
	else
	{
	    d = -d;
	    if (blen + d <= alen + b->prec)
	    {
		bmant = FpartLsl (bmant, d);
		exp = a->exp;
		prec = a->prec;
		if (b->prec + d < prec)
		    prec = b->prec + d;
	    }
	}
	if (prec)
	{
	    mant = FpartAdd (amant, bmant, negate);
	    ret = NewFloat (mant, exp, prec);
	}
    }
    if (!ret)
    {
	if (dist->sign == Negative)
	{
	    ret = bv;
	    if (negate)
		ret = NewFloat (NewFpart (SignNegate (bv->floats.mant->sign),
					  bv->floats.mant->mag),
				bv->floats.exp,
				bv->floats.prec);
	}
	else
	    ret = av;
    }
    RETURN (ret);
}

static Value
FloatPlus (Value av, Value bv, int expandOk)
{
    return FloatAdd (av, bv, expandOk, False);
}

static Value
FloatMinus (Value av, Value bv, int expandOk)
{
    return FloatAdd (av, bv, expandOk, True);
}

static Value
FloatTimes (Value av, Value bv, int expandOk)
{
    ENTER ();
    Float	*a = &av->floats, *b = &bv->floats;
    Fpart	*mant;
    Fpart	*exp;
    unsigned	prec;
    
    mant = FpartMult (a->mant, b->mant);
    exp = FpartAdd (a->exp, b->exp, False);
    if (a->prec < b->prec)
	prec = a->prec;
    else
	prec = b->prec;
    RETURN (NewFloat (mant, exp, prec));
}

static Value
FloatDivide (Value av, Value bv, int expandOk)
{
    ENTER ();
    Float	*a = &av->floats, *b = &bv->floats;
    Fpart	*mant;
    Fpart	*amant = a->mant, *bmant = b->mant;
    Fpart	*exp;
    unsigned	prec;
    unsigned	iprec, alen;

    if (FpartZero (b->mant))
    {
	RaiseStandardException (exception_divide_by_zero,
				"real divide by zero",
				2, av, bv);
	RETURN (Zero);
    }
    DebugF ("Dividend ", a);
    DebugF ("Divisor ", b);
    if (a->prec < b->prec)
	prec = a->prec;
    else
	prec = b->prec;
    iprec = prec + FpartLength (bmant) + 1;
    alen = FpartLength (amant);
    exp = b->exp;
    if (alen < iprec)
    {
	amant = FpartLsl (amant, iprec - alen);
	exp = FpartAdd (NewIntFpart (iprec-alen), exp, False);
    }
    exp = FpartAdd (a->exp, exp, True);
    DebugFp ("amant ", amant);
    DebugFp ("bmant ", bmant);
    mant = FpartDivide (amant, bmant);
    DebugFp ("mant ", mant);
    DebugFp ("exp ", exp);
    RETURN (NewFloat (mant, exp, prec));
}

static Value
FloatMod (Value av, Value bv, int expandOk)
{
    ENTER ();
    Value   q;

    q = Floor (Divide (av, bv));
    av = Minus (av, Times (q, bv));
    RETURN (av);
}

static Value
FloatLess (Value av, Value bv, int expandOk)
{
    ENTER ();
    Value	ret;
    Float	*a = &av->floats, *b = &bv->floats;
    
    if (FpartEqual (a->mant, zero_fpart))
    {
	if (b->mant->sign == Positive && 
	    !FpartEqual (b->mant, zero_fpart))
	    ret = One;
	else
	    ret = Zero;
    }
    else if (FpartEqual (b->mant, zero_fpart))
    {
	if (a->mant->sign == Negative)
	    ret = One;
	else
	    ret = Zero;
    }
    else if (FpartEqual (a->exp, b->exp))
    {
	ret = Zero;
	if (FpartLess (a->mant, b->mant))
	    ret = One;
    }
    else
    {
	av = FloatMinus (av, bv, expandOk);
	ret = Zero;
	if (av->floats.mant->sign == Negative)
	    ret = One;
    }
    RETURN (ret);
}

static Value
FloatEqual (Value av, Value bv, int expandOk)
{
    ENTER ();
    Value	ret;
    Float	*a = &av->floats, *b = &bv->floats;

    if (FpartEqual (a->exp, b->exp))
    {
	ret = Zero;
	if (FpartEqual (a->mant, b->mant))
	    ret = One;
    }
    else
    {
	av = FloatMinus (av, bv, expandOk);
	ret = Zero;
	if (NaturalZero (av->floats.mant->mag))
	    ret = One;
    }
    RETURN (ret);
}

static Value
FloatNegate (Value av, int expandOk)
{
    ENTER ();
    Float   *a = &av->floats;

    RETURN (NewFloat (FpartNegate (a->mant), a->exp, a->prec));
}

static Value
FloatInteger (Value av)
{
    ENTER ();
    Float	*a = &av->floats;
    Natural	*mag;
    int		dist;

    /*
     * Can only reduce floats that are integral
     *
     * Ensure that the precision of the number holds every bit
     * This requires that the precision of the representation
     * be no smaller than length of the numbers plus the
     * number of implied zeros.
     *
     *  precision >= length (mant) + exponent
     *  precision - length (mant) >= exponent
     * !(precision - length (mant) < exponent)
     *
     * The canonical representation ensures that length <= prec
     */
    if (a->exp->sign == Positive &&
	!NaturalLess (NewNatural (a->prec - FpartLength (a->mant)),
		      a->exp->mag))
    {
	mag = a->mant->mag;
	dist = NaturalToInt (a->exp->mag);
	if (dist)
	    mag = NaturalLsl (mag, dist);
	av = Reduce (NewInteger (a->mant->sign, mag));
    }
    RETURN (av);
}

static Value
FloatFloor (Value av, int expandOk)
{
    ENTER ();
    Float   *a = &av->floats;
    Fpart   *mant;
    Fpart   *exp;
    int	    d;

    if (a->exp->sign == Positive)
	RETURN (FloatInteger (av));
    if (NaturalLess (NewNatural (a->prec), a->exp->mag))
	RETURN (Zero);
    d = NaturalToInt (a->exp->mag);
    mant = FpartRsl (a->mant, d);
    if (d && a->mant->sign == Negative)
	mant = FpartAdd (mant, one_fpart, False);
    exp = zero_fpart;
    RETURN (FloatInteger (NewFloat (mant, exp, a->prec - d)));
}
   
static Value
FloatCeil (Value av, int expandOk)
{
    ENTER ();
    Float   *a = &av->floats;
    Fpart   *mant;
    Fpart   *exp;
    int	    d;

    if (a->exp->sign == Positive)
	RETURN (FloatInteger (av));
    if (NaturalLess (NewNatural (a->prec), a->exp->mag))
	RETURN (Zero);
    d = NaturalToInt (a->exp->mag);
    mant = FpartRsl (a->mant, d);
    if (d && a->mant->sign == Positive)
	mant = FpartAdd (mant, one_fpart, False);
    exp = zero_fpart;
    RETURN (FloatInteger (NewFloat (mant, exp, a->prec - d)));
}
    
static Value
FloatPromote (Value av, Value bv)
{
    ENTER ();
    int	prec;

    if (av->value.tag != type_float)
    {
	if (bv && bv->value.tag == type_float)
	    prec = bv->floats.prec;
	else
	    prec = DEFAULT_FLOAT_PREC;
	av = NewValueFloat (av, prec);
    }
    RETURN (av);
}

static Value
FloatReduce (Value av)
{
    return av;
}

#if 0
static void
FpartPrint (Value f, Fpart *fp, int base)
{
    ENTER ();
    int	    print_width;
    
    if (fp->sign == Negative)
	FileOutput (f, '-');
    FilePuts (f, NaturalSprint (0, fp->mag, base, &print_width));
    EXIT ();
}
#endif

/*
 *  1/2 <= value / 2^exp2 < 1
 *  1/base <= value / base^expbase < 1
 *
 *  2^(exp2-1) <= value < 2^exp2
 *
 *  assign value = 2^(exp2-1)
 *
 *  then
 *
 *	1/base <= 2^(exp2-1) / base^expbase < 1
 *
 *	1 <= 2^(exp2-1) / (base^(expbase-1)) < base
 *
 *	-log(base) <= (exp2-1) * log(2) - expbase * log(base) < 1
 *	
 *  ignoring the right inequality
 *
 *	0 <= (exp2 - 1) * log(2) - (expbase-1) * log(base)
 *	(expbase - 1) * log(base) <= (exp2 - 1) * log(2)
 *	(expbase - 1) <= (exp2 - 1) * log(2) / log(base);
 *	expbase <= (exp2 - 1) * log(2) / log(base) + 1;
 *	expbase = floor ((exp2 - 1) * log(2) / log(base) + 1);
 *
 *  Depending on value, expbase may need an additional digit
 */

#if 0
static Bool
NaturalBitSet (Natural *n, int i)
{
    int	d = i / LBASE2;
    int	b = i & LBASE2;
    
    return d < NaturalLength (n) && (NaturalDigits(n)[d] & 1 << b);
}
#endif


static Value
FloatExp (Value exp2, Value *ratio, int ibase)
{
    ENTER ();
    double  dscale;
    Value   scale;
    Value   r;
    Value   min, max, mean, nmean;
    Value   pow2;
    Value   base;
    Value   two;
    Bool    done;

    DebugV ("exp2", exp2);
    two = NewInt (2);
    base = NewInt (ibase);
    /*
     * Compute expbase, this is a bit tricky as log is only
     * available in floats
     */
    dscale = log(2) / log(ibase) * MAXINT;
    scale = Divide (NewInt ((int) dscale),
		    NewInt (MAXINT));
    min = Floor (Plus (Times (Minus (exp2,  One), scale), One));
    if (Negativep (min))
	max = Divide (min, two);
    else
	max = Times (min, two);
    
    pow2 = Pow (two, Minus (exp2, One));
    
    mean = 0;
    done = False;
    do
    {
	nmean = Div (Plus (min, max), two);
	if (mean && True(Equal (nmean, mean)))
	{
	    nmean = Plus (nmean, One);
	    done = True;
	}
	mean = nmean;
	DebugV ("min ", min);
	DebugV ("mean", mean);
	DebugV ("max ", max);
	r = Divide (pow2, Pow (base, Minus (mean, One)));
	if (done)
	    break;
	if (True (Less (One, r)))
	    min = mean;
	else
	    max = mean;
    } while (False (Equal (max, min)));
    mean = Minus (mean, One);
/*    r = Divide (pow2, Pow (base, Minus (mean, One)));*/
    r = Divide (Pow (two, exp2), Pow (base, mean));
/*    r = Divide (pow2, Pow (base, mean)); */
    EXIT ();
    REFERENCE (mean);
    REFERENCE (r);
    *ratio = r;
    return mean;
}

static Bool
FloatPrint (Value f, Value fv, char format, int base, int width, int prec, unsigned char fill)
{
    ENTER ();
    Float	*a = &fv->floats;
    Value	expbase;
    Fpart	*exp;
    Natural	*int_n;
    Natural	*frac_n;
    Value	ratio;
    Value	down;
    Value	fratio;
    Value	m;
    Value	int_part;
    Value	frac_part;
    unsigned	length;
    int		mant_prec;
    int		frac_prec;
    int		dig_max;
    int		exp_width;
    int		int_width;
    int		frac_width;
    int		print_width;
    Bool	negative;
    char	*int_buffer;
    char	*int_string;
    char	*frac_buffer;
    char	*frac_string;
    char	*exp_string = 0;
    
    if (base <= 0)
	base = 10;
    
    if (prec == DEFAULT_OUTPUT_PRECISION)
	prec = 15;
    
    mant_prec = a->prec * log(2) / log(base);

    DebugFp ("mant", a->mant);
    DebugFp ("exp ", a->exp);
    
    length = FpartLength (a->mant);
    expbase = FloatExp (Plus (NewInt (length), 
			      NewInteger (a->exp->sign,
					  a->exp->mag)),
			&ratio,
			base);
    DebugV ("expbase", expbase);
    DebugV ("ratio  ", ratio);
    down = Pow (NewInt (2),
		NewInt ((int) length));
    DebugV ("down   ", down);
    fratio = Divide (ratio, down);
    DebugV ("fratio ", fratio);
    negative = a->mant->sign == Negative;
    m = NewInteger (Positive, a->mant->mag);
    if (FpartLength (a->mant) == a->prec)
	m = Plus (m, One);
    m = Times (m, fratio);
    if (True (Less (m, One)))
    {
	m = Times (m, NewInt (base));
	expbase = Minus (expbase, One);
    }
    exp = NewValueFpart (expbase);
    switch (format) {
    case 'e':
    case 'E':
    case 'f':
	break;
    default:
	dig_max = prec;
	if ((exp->sign == Positive &&
	    !NaturalLess (exp->mag, NewNatural (dig_max))) ||
	    (exp->sign == Negative &&
	     NaturalLess (NewNatural (4), exp->mag)))
	{
	    format = 'e';
	}
	else
	{
	    format = 'f';
	}
    }
    
    if (format == 'f')
    {
	m = Times (m, Pow (NewInt (base), expbase));
	exp_width = 0;
	if (prec == INFINITE_OUTPUT_PRECISION)
	{
	    prec = mant_prec;
	    if (expbase->value.tag == type_int)
	    {
		if (expbase->ints.value < 0)
		    prec -= expbase->ints.value;
		else if (expbase->ints.value > prec)
		    prec = expbase->ints.value;
	    }
	}
    }
    else
    {
	exp_string = NaturalSprint (0, exp->mag, base, &exp_width);
	exp_width++;
	if (exp->sign == Negative)
	    exp_width++;
	if (prec == INFINITE_OUTPUT_PRECISION)
	    prec = mant_prec;
    }
	
    int_part = Floor (m);
    frac_part = Minus (m, int_part);
	
    if (int_part->value.tag == type_integer)
	int_n = int_part->integer.mag;
    else
	int_n = NewNatural (int_part->ints.value);

    int_width = NaturalEstimateLength (int_n, base);
    if (negative)
	int_width++;
    
    int_buffer = malloc (int_width + 1);
    int_string = NaturalSprint (int_buffer + int_width + 1,
				int_n, base, &int_width);
    
    frac_prec = mant_prec - int_width;
    if (*int_string == '0')
	frac_prec++;
    
    if (negative)
    {
	*--int_string = '-';
	int_width++;
    }
    
    frac_width = prec - int_width - exp_width;
    
    if (frac_prec + 1 < frac_width || prec == INFINITE_OUTPUT_PRECISION)
	frac_width = frac_prec + 1;
    if (frac_width < 2)
	frac_width = 0;
    frac_buffer = 0;
    frac_string = 0;
    if (frac_width && !Zerop (frac_part))
    {
	int	frac_wrote;
	
	frac_part = Floor (Times (frac_part, Pow (NewInt (base), 
						  NewInt (frac_width - 1))));
	if (frac_part->value.tag == type_integer)
	    frac_n = frac_part->integer.mag;
	else
	    frac_n = NewNatural (frac_part->ints.value);
	
	frac_buffer = malloc (frac_width + 1);
	frac_string = NaturalSprint (frac_buffer + frac_width + 1,
				     frac_n, base, &frac_wrote);
	while (frac_wrote < frac_width - 1)
	{
	    *--frac_string = '0';
	    frac_wrote++;
	}
	*--frac_string = '.';
    }

    print_width = int_width + frac_width + exp_width;
    while (width > print_width)
    {
	FileOutput (f, fill);
	width--;
    }
    
    FilePuts (f, int_string);
    if (frac_string)
	FilePuts (f, frac_string);

    if (exp_width)
    {
	FilePuts (f, "e");
	if (exp->sign == Negative)
	    FilePuts (f, "-");
	FilePuts (f, exp_string);
    }
    free (int_buffer);
    if (frac_buffer)
	free (frac_buffer);
    
    EXIT ();
    return True;
}

static void
FloatMark (void *object)
{
    Float   *f = object;

    MemReference (f->mant);
    MemReference (f->exp);
}

ValueType   FloatType = {
    { FloatMark, 0 },	/* base */
    {			/* binary */
	FloatPlus,
	FloatMinus,
	FloatTimes,
	FloatDivide,
	NumericDiv,
	FloatMod,
	FloatLess,
	FloatEqual,
	0,
	0,
    },
    {			/* unary */
	FloatNegate,
	FloatFloor,
	FloatCeil,
    },
    FloatPromote,
    FloatReduce,
    FloatPrint
};

Value
NewFloat (Fpart *mant, Fpart *exp, unsigned prec)
{
    ENTER ();
    unsigned	bits, dist;
    Value	ret;

    DebugFp ("New mant", mant);
    DebugFp ("New exp ", exp);
    /*
     * Trim to specified precision
     */
    bits = FpartLength (mant);
    if (bits > prec)
    {
	dist = bits - prec;
	exp = FpartAdd (exp, NewIntFpart (dist), False);
	mant = FpartRsl (mant, dist);
    }
    /*
     * Canonicalize by shifting to a 1 in the LSB
     */
    dist = FpartZeros (mant);
    if (dist)
    {
	exp = FpartAdd (exp, NewIntFpart (dist), False);
	mant = FpartRsl (mant, dist);
    }
    bits = FpartLength (mant);
    if (bits == 0)
	exp = mant = zero_fpart;
    DebugFp ("Can mant", mant);
    DebugFp ("Can exp ", exp);
    ret = ALLOCATE (&FloatType.data, sizeof (Float));
    ret->value.tag = type_float;
    ret->floats.mant = mant;
    ret->floats.exp = exp;
    ret->floats.prec = prec;
    RETURN (ret);
}

Value
NewIntFloat (int i, unsigned prec)
{
    ENTER ();
    RETURN (NewFloat (NewIntFpart (i), zero_fpart, prec));
}

Value
NewIntegerFloat (Integer *i, unsigned prec)
{
    ENTER ();
    Fpart   *mant;

    mant = NewFpart (i->sign, i->mag);
    RETURN (NewFloat (mant, zero_fpart, prec));
}

Value
NewNaturalFloat (Sign sign, Natural *n, unsigned prec)
{
    ENTER ();
    Fpart   *mant;

    mant = NewFpart (sign, n);
    RETURN (NewFloat (mant, zero_fpart, prec));
}

Value
NewRationalFloat (Rational *r, unsigned prec)
{
    ENTER ();
    Value   num, den;
    
    num = NewNaturalFloat (r->sign, r->num, prec);
    den = NewNaturalFloat (Positive, r->den, prec);
    RETURN (FloatDivide (num, den, 1));
}

Value
NewValueFloat (Value av, unsigned prec)
{
    ENTER ();

    switch (av->value.tag) {
    case type_int:
	av = NewIntFloat (av->ints.value, prec);
	break;
    case type_integer:
	av = NewIntegerFloat (&av->integer, prec);
	break;
    case type_rational:
	av = NewRationalFloat (&av->rational, prec);
	break;
    case type_float:
        av = NewFloat (av->floats.mant, av->floats.exp, prec);
	break;
    default:
	break;
    }
    RETURN (av);
}
