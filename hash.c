/*
 * Copyright © 2003 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include "nickle.h"

/*
 * From Knuth -- a good choice for hash/rehash values is p, p-2 where
 * p and p-2 are both prime.  These tables are sized to have an extra 10%
 * free to avoid exponential performance degradation as the hash table fills
 */

static HashSetRec hashSets[] = {
    { 2,		5,		3	  },
    { 4,		7,		5	  },
    { 8,		13,		11	  },
    { 16,		19,		17	  },
    { 32,		43,		41        },
    { 64,		73,		71        },
    { 128,		151,		149       },
    { 256,		283,		281       },
    { 512,		571,		569       },
    { 1024,		1153,		1151      },
    { 2048,		2269,		2267      },
    { 4096,		4519,		4517      },
    { 8192,		9013,		9011      },
    { 16384,		18043,		18041     },
    { 32768,		36109,		36107     },
    { 65536,		72091,		72089     },
    { 131072,		144409,		144407    },
    { 262144,		288361,		288359    },
    { 524288,		576883,		576881    },
    { 1048576,		1153459,	1153457   },
    { 2097152,		2307163,	2307161   },
    { 4194304,		4613893,	4613891   },
    { 8388608,		9227641,	9227639   },
    { 16777216,		18455029,	18455027  },
    { 33554432,		36911011,	36911009  },
    { 67108864,		73819861,	73819859  },
    { 134217728,	147639589,	147639587 },
    { 268435456,	295279081,	295279079 },
    { 536870912,	590559793,	590559791 },
    { 1073741824,	1181116273,	1181116271},
    { 2147483648ul,	2362232233ul,	2362232231ul}
};

#define NHASHSETS	(sizeof(hashSets)/sizeof(hashSets[0]))

static Value *
Find (HashTablePtr ht, Value hash, Value key)
{
    HashSetPtr	    hs = ht->hashSet;
    HashValue	    size = hs->size;
    HashValue	    h = ValueInt (hash);
    HashValue	    elt = h % size;
    HashValue	    step = 0;
    Value	    *elts = BoxElements (ht->elts);
    Value	    *er, *del = 0;

    for (;;)
    {
	er = &elts[elt * HashEltSize];
	if (!HashEltKey(er))
	{
	    /* check for a deleted entry */
	    if (HashEltValue (er))
	    {
		/* save first deleted entry found */
		if (!del)
		    del = er;
		else if (er == del)
		    break;
	    }
	    else
	    {
		/* pull reference as far forward on the chain as posible */
		if (del)
		    er = del;
		break;
	    }
	}
	else if (HashEltHash(er) == hash && 
		 Equal(HashEltKey(er), key) == TrueVal)
	{
	    break;
	}
	if (!step)
	{
	    step = h % hs->rehash;
	    if (!step)
		step = 1;
	}
	elt += step;
	if (elt >= size)
	    elt -= size;
    }
    return er;
}

static void
Rehash (BoxPtr old, HashTablePtr ht)
{
    Value	*o, *n;
    HashValue	h;
    
    o = BoxElements (old);
    ht->count = 0;
    for (h = old->nvalues / HashEltSize; h > 0; h--)
    {
	if (HashEltValid (o))
	{
	    /* XXX must rewrite references */
	    n = Find (ht, HashEltHash(o), HashEltKey(o));
	    HashEltCopy (n, o);
	    ht->count++;
	}
	HashEltStep (o);
    }
}

static void
Resize (HashTablePtr ht, const HashSetPtr hs)
{
    ENTER ();
    BoxPtr	old;
    
    if (ht->hashSet == hs)
	return;

    old = ht->elts;
    ht->elts = NewBox (False, False, hs->size * HashEltSize,
		       ht->type);
    ht->hashSet = hs;
    if (old)
	Rehash (old, ht);
    EXIT ();
}

#if HAVE_STDINT_H
typedef uint32_t	crc32_t;
#else
typedef unsigned int	crc32_t;
#endif

static crc32_t crc32_table[256];

static void
generate_crc32_table(void)
{
    crc32_t	c, p;
    int		n, m;

    p = 0xedb88320;
    for (n = 0; n < 256; n++)
    {
	c = n;
	for (m = 0; m < 8; m++)
	    c = (c >> 1) ^ ((c & 1) ? p : 0);
	crc32_table[n] = c;
    }
}

HashValue
HashCrc32 (unsigned char *bytes, int nbytes)
{
    crc32_t	crc32 = ~0;
    if (crc32_table[1] == 0) abort ();
    while (nbytes--)
	crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *bytes++) & 0xff];
    return (HashValue) ~crc32;
}

int
HashInit (void)
{
    generate_crc32_table ();
    return 1;
}

static Value
HashEqual (Value av, Value bv, int expandOk)
{
    HashTable	*at = &av->hash;
    HashTable	*bt = &bv->hash;
    HashValue	i;
    Value	*ae, *be;

    /* if they have different numbers of valid elements, they're not equal */
    if (at->count != bt->count)
	return FalseVal;
    ae = BoxElements (at->elts);
    for (i = 0; i < at->hashSet->size; i++)
    {
	if (HashEltValid (ae)) 
	{
	    be = Find (bt, HashEltHash(ae), HashEltKey (ae));
	    if (!be || !HashEltValid (be))
		return FalseVal;
	    if (Equal (HashEltValue (be), HashEltValue (ae)) != TrueVal)
		return FalseVal;
	}
	HashEltStep (ae);
    }
    return TrueVal;
}

static Bool
HashPrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    HashTable	*ht = &av->hash;
    HashValue	h;
    Value	*e;
    Bool	first = True;
    Bool	pretty = format == 'v' || format == 'g';
    
    if (pretty)
    {
	FilePuts (f, "(");
	FilePutBaseType (f, ht->type, False);
	FilePuts (f, " ");
	FilePuts (f, "[");
	FilePutType (f, ht->keyType, False);
	FilePuts (f, "]");
	FilePutSubscriptType (f, ht->type, False);
	FilePuts (f, ") {");
    }
    e = BoxElements (ht->elts);
    for (h = ht->hashSet->size; h-- > 0; )
    {
	if (HashEltValid (e))
	{
	    if (pretty)
	    {
		if (!first)
		    FilePuts (f, ", ");
		else
		    FilePuts (f, " ");
		Print (f, HashEltKey (e), format, base, width, prec, fill);
		FilePuts (f, " => ");
	    }
	    else if (!first)
		FilePuts (f, " ");
	    Print (f, HashEltValue (e), format, base, width, prec, fill);
	    first = False;
	}
	HashEltStep (e);
    }
    if (pretty)
    {
	if (ht->def)
	{
	    if (!first)
		FilePuts (f, ",");
	    FilePuts (f, " => ");
	    Print (f, ht->def, format, base, width, prec, fill);
	    first = False;
	}
	if (!first)
	    FilePuts (f, " ");
	FilePuts (f, "}");
    }
    return True;
}

static HashValue
HashHash (Value av)
{
    HashTable	*ht = &av->hash;
    HashValue	h;
    Value	*e;
    HashValue	hash = 0;
    
    e = BoxElements (ht->elts);
    for (h = ht->hashSet->size; h-- > 0; )
    {
	if (HashEltValue (e))
	    hash ^= ValueInt (HashEltHash (e));
	HashEltStep (e);
    }
    return hash;
}

static void
HashMark (void *object)
{
    HashTable	*ht = (HashTable *) object;

    MemReference (ht->type);
    MemReference (ht->keyType);
    MemReference (ht->elts);
    MemReference (ht->def);
}

ValueRep    HashRep = { 
    { HashMark, 0, "HashRep" },
    rep_hash,
    {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	HashEqual,
	0,
	0,
    },
    {
	0,
    },
    0,
    0,
    HashPrint,
    0,
    HashHash,
};

Value
NewHash (Bool constant, TypePtr keyType, TypePtr type)
{
    ENTER ();
    Value   ret = ALLOCATE (&HashRep.data, sizeof (HashTable));
    ret->hash.hashSet = 0;
    ret->hash.count = 0;
    ret->hash.type = type;
    ret->hash.keyType = keyType;
    ret->hash.elts = 0;
    ret->hash.def = 0;
    Resize (&ret->hash, &hashSets[0]);
    RETURN (ret);
}

Value
HashGet (Value hv, Value key)
{
    HashTablePtr    ht = &hv->hash;
    Value	    hash = ValueHash (key);
    Value	    *he;
    Value	    value;

    he = Find (ht, hash, key);
    if (!HashEltValid (he))
    {
	if (!ht->def)
	{
	    RaiseStandardException (exception_uninitialized_value, 0);
	    return (Void);
	}
	if (ht->count >= ht->hashSet->entries && 
	    ht->hashSet != &hashSets[NHASHSETS - 1])
	{
	    Resize (ht, ht->hashSet + 1);
	}
	ht->count++;
	HashEltHash(he) = hash;
	HashEltKey(he) = Copy (key);
	HashEltValue(he) = Copy(ht->def);
    }
    value = HashEltValue (he);
    if (!value)
    {
	RaiseStandardException (exception_uninitialized_value, 0);
	return (Void);
    }
    return value;
}

void
HashSet (Value hv, Value key, Value value)
{
    HashTablePtr    ht = &hv->hash;
    Value	    hash = ValueHash (key);
    Value	    *he;

    if (ht->count >= ht->hashSet->entries && 
	ht->hashSet != &hashSets[NHASHSETS - 1])
    {
	Resize (ht, ht->hashSet + 1);
    }
    he = Find (ht, hash, key);
    if (!HashEltValid (he))
	ht->count++;
    HashEltHash (he) = hash;
    HashEltKey (he) = Copy (key);
    HashEltValue (he) = Copy (value);
}

void
HashSetDef (Value hv, Value def)
{
    HashTablePtr    ht = &hv->hash;

    ht->def = Copy(def);
}

Value
HashRef (Value hv, Value key)
{
    ENTER ();
    HashTablePtr    ht = &hv->hash;
    Value	    *he;
    Value	    hash = ValueHash (key);

    if (ht->count == ht->hashSet->entries && 
	ht->hashSet != &hashSets[NHASHSETS - 1])
    {
	Resize (ht, ht->hashSet + 1);
    }
    he = Find (ht, hash, key);
    if (!HashEltValid (he))
    {
	ht->count++;
	HashEltHash (he) = hash;
	HashEltKey (he) = Copy (key);
	if (ht->def)
	    HashEltValue (he) = Copy(ht->def);
    }
    RETURN (NewRef (ht->elts, 
		    &HashEltValue(he) - BoxElements (ht->elts)));
}

Value
HashTest (Value hv, Value key)
{
    HashTablePtr    ht = &hv->hash;
    Value	    *he;
    Value	    hash = ValueHash (key);

    he = Find (ht, hash, key);
    return HashEltValid (he) ? TrueVal : FalseVal;
}

void
HashDelete (Value hv, Value key)
{
    HashTablePtr    ht = &hv->hash;
    Value	    *he;
    Value	    hash = ValueHash (key);

    he = Find (ht, hash, key);
    if (HashEltValid (he))
    {
	/* mark this entry as deleted -- value Void, key NULL */
	HashEltHash (he) = 0;
	HashEltKey (he) = 0;
	HashEltValue (he) = Void;
	--ht->count;
    }
}

Value
HashKeys (Value hv)
{
    ENTER ();
    HashTablePtr    ht = &hv->hash;
    int		    dim = ht->count;
    Value	    keys = NewArray (False, True, ht->keyType, 1, &dim);
    HashValue	    h;
    int		    i = 0;
    Value	    *e = BoxElements (ht->elts);

    for (h = ht->hashSet->size; h > 0; h--)
    {
	if (HashEltValid (e)) 
	{
	    ArrayValueSet (&keys->array, i, HashEltKey (e));
	    i++;
	}
	HashEltStep (e);
    }
    if (i != dim)
	ArrayResize (keys, 0, i);
    RETURN (keys);
}

Value
HashCopy (Value hv)
{
    ENTER ();
    Value   new = NewHash (False, hv->hash.keyType, hv->hash.type);
    new->hash.def = hv->hash.def;
    Resize (&new->hash, hv->hash.hashSet);
    Rehash (hv->hash.elts, &new->hash);
    RETURN (new);
}
