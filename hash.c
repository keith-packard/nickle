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
    { 2,            5,            3            },
    { 4,            7,            5            },
    { 8,            13,           11           },
    { 16,           19,           17           },
    { 32,           43,           41           },
    { 64,           73,           71           },
    { 128,          151,          149          },
    { 256,          283,          281          },
    { 512,          571,          569          },
    { 1024,         1153,         1151         },
    { 2048,         2269,         2267         },
    { 4096,         4519,         4517         },
    { 8192,         9013,         9011         },
    { 16384,        18043,        18041        },
    { 32768,        36109,        36107        },
    { 65536,        72091,        72089        },
    { 131072,       144409,       144407       },
    { 262144,       288361,       288359       },
    { 524288,       576883,       576881       },
    { 1048576,      1153459,      1153457      },
    { 2097152,      2307163,      2307161      },
    { 4194304,      4613893,      4613891      },
    { 8388608,      9227641,      9227639      },
    { 16777216,     18455029,     18455027     },
    { 33554432,     36911011,     36911009     },
    { 67108864,     73819861,     73819859     },
    { 134217728,    147639589,    147639587    },
    { 268435456,    295279081,    295279079    },
    { 536870912,    590559793,    590559791    },
    { 1073741824,   1181116273,   1181116271   },
#if PTRDIFF_MAX > 2362232233
    { 2147483648ul, 2362232233ul, 2362232231ul }
#endif
};

#define NHASHSETS (sizeof(hashSets) / sizeof(hashSets[0]))

static void
HashEltsMark(void *object)
{
    HashEltsPtr elts = object;
    ptrdiff_t   i;

    for (i = 0; i < elts->size; i++) {
        MemReference(elts->elts[i].hash);
        MemReference(elts->elts[i].key);
        MemReference(elts->elts[i].box);
    }
}

static DataType HashEltsType = { HashEltsMark, 0, "HashEltsType" };

static HashEltsPtr
NewHashElts(ptrdiff_t size)
{
    ENTER();
    HashEltsPtr elts;

    elts = ALLOCATE(&HashEltsType, sizeof(HashElts) + (size_t)size * sizeof(HashElt));
    elts->size = size;
    memset(elts->elts, '\0', (size_t)size * sizeof(HashElt));
    RETURN(elts);
}

/* Give a new element a box of its own holding 'value' */
static void
HashEltSetBox(HashTablePtr ht, HashEltPtr he, Value value)
{
    ENTER();
    BoxPtr box = NewBox(False, False, 1, ht->type);

    BoxValueSet(box, 0, value);
    he->box = box;
    EXIT();
}

static HashEltPtr
Find(HashTablePtr ht, Value hash, Value key)
{
    HashSetPtr hs = ht->hashSet;
    ptrdiff_t  size = hs->size;
    HashValue  h = (HashValue)ValueInt(hash);
    ptrdiff_t  elt = (ptrdiff_t)(h % (HashValue)size);
    ptrdiff_t  step = 0;
    HashEltPtr er, del = 0;

    for (;;) {
        er = &ht->elts->elts[elt];
        if (!er->key) {
            /* check for a deleted entry */
            if (HashEltChained(er)) {
                /* save first deleted entry found */
                if (!del)
                    del = er;
                else if (er == del)
                    break;
            } else {
                /* pull reference as far forward on the chain as posible */
                if (del)
                    er = del;
                break;
            }
        } else if (er->hash == hash && Equal(er->key, key) == TrueVal) {
            break;
        }
        if (!step) {
            step = (ptrdiff_t)(h % (HashValue)hs->rehash);
            if (!step)
                step = 1;
        }
        elt += step;
        if (elt >= size)
            elt -= size;
    }
    return er;
}

/* Move the valid elements of 'old' into 'ht', boxes and all */
static void
Rehash(HashEltsPtr old, HashTablePtr ht)
{
    ptrdiff_t h;

    ht->count = 0;
    for (h = 0; h < old->size; h++) {
        HashEltPtr o = &old->elts[h];

        if (HashEltValid(o)) {
            *Find(ht, o->hash, o->key) = *o;
            ht->count++;
        }
    }
}

static void
Resize(HashTablePtr ht, const HashSetPtr hs)
{
    HashEltsPtr old;

    if (ht->hashSet == hs)
        return;

    ENTER();
    old = ht->elts;
    /* Rehash compares keys, which may collect; keep the old elements until it's done */
    if (old)
        REFERENCE(old);
    ht->elts = NewHashElts(hs->size);
    ht->hashSet = hs;
    if (old)
        Rehash(old, ht);
    EXIT();
}

#ifdef HAVE_STDINT_H
typedef uint32_t crc32_t;
#else
typedef unsigned int crc32_t;
#endif

static crc32_t crc32_table[256];

static void
generate_crc32_table(void)
{
    crc32_t c, p;
    int     n, m;

    p = 0xedb88320;
    for (n = 0; n < 256; n++) {
        c = (crc32_t)n;
        for (m = 0; m < 8; m++)
            c = (c >> 1) ^ ((c & 1) ? p : 0);
        crc32_table[n] = c;
    }
}

HashValue
HashCrc32(unsigned char *bytes, size_t nbytes)
{
    crc32_t crc32 = ~(crc32_t)0;
    if (crc32_table[1] == 0)
        abort();
    while (nbytes--)
        crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *bytes++) & 0xff];
    return (HashValue)~crc32;
}

int
HashInit(void)
{
    generate_crc32_table();
    return 1;
}

static Value
HashEqual(Value av, Value bv, int expandOk)
{
    HashTable *at = &av->hash;
    HashTable *bt = &bv->hash;
    ptrdiff_t  i;
    HashEltPtr ae, be;

    (void)expandOk;
    /* if they have different numbers of valid elements, they're not equal */
    if (at->count != bt->count)
        return FalseVal;
    for (i = 0; i < at->elts->size; i++) {
        ae = &at->elts->elts[i];
        if (HashEltValid(ae)) {
            be = Find(bt, ae->hash, ae->key);
            if (!be || !HashEltValid(be))
                return FalseVal;
            /* An element made by referencing it has no value; arrays raise for these too */
            if (!HashEltValue(ae) || !HashEltValue(be)) {
                RaiseStandardException(exception_uninitialized_value, 1,
                                       NewStrString("uninitialized value"));
                return FalseVal;
            }
            if (Equal(HashEltValue(be), HashEltValue(ae)) != TrueVal)
                return FalseVal;
        }
    }
    return TrueVal;
}

static Bool
HashPrint(Value f, Value av, uint32_t format, int base, int width, int prec, uint32_t fill)
{
    HashTable *ht = &av->hash;
    ptrdiff_t  h;
    HashEltPtr e;
    Bool       first = True;
    Bool       pretty = format == 'v' || format == 'g';

    if (pretty) {
        FilePuts(f, "(");
        FilePutBaseType(f, ht->type, False);
        FilePuts(f, " ");
        FilePuts(f, "[");
        FilePutType(f, ht->keyType, False);
        FilePuts(f, "]");
        FilePutSubscriptType(f, ht->type, False);
        FilePuts(f, ") {");
    }
    for (h = 0; h < ht->elts->size; h++) {
        e = &ht->elts->elts[h];
        if (HashEltValid(e)) {
            if (pretty) {
                if (!first)
                    FilePuts(f, ", ");
                else
                    FilePuts(f, " ");
                Print(f, e->key, format, base, width, prec, fill);
                FilePuts(f, " => ");
            } else if (!first)
                FilePuts(f, " ");
            Print(f, HashEltValue(e), format, base, width, prec, fill);
            first = False;
        }
    }
    if (pretty) {
        if (ht->def) {
            if (!first)
                FilePuts(f, ",");
            FilePuts(f, " => ");
            Print(f, ht->def, format, base, width, prec, fill);
            first = False;
        }
        if (!first)
            FilePuts(f, " ");
        FilePuts(f, "}");
    }
    return True;
}

static HashValue
HashHash(Value av)
{
    HashTable *ht = &av->hash;
    ptrdiff_t  h;
    HashValue  hash = 0;

    for (h = 0; h < ht->elts->size; h++) {
        HashEltPtr e = &ht->elts->elts[h];

        /* Skip elements with no value, so arrays holding them can be keys */
        if (HashEltValid(e) && HashEltValue(e))
            hash ^= ValueHashValue(e->key);
    }
    return hash;
}

static void
HashMark(void *object)
{
    HashTable *ht = (HashTable *)object;

    MemReference(ht->type);
    MemReference(ht->keyType);
    MemReference(ht->elts);
    MemReference(ht->def);
}

ValueRep HashRep = {
    { HashMark, 0, "HashRep" },
    rep_hash,
    {
     0, 0,
     0, 0,
     0, 0,
     0, HashEqual,
     0, 0,
     },
    {
     0, },
    0,
    0,
    HashPrint,
    0,
    HashHash,
};

Value
NewHash(Bool constant, TypePtr keyType, TypePtr type)
{
    ENTER();
    Value ret = ALLOCATE(&HashRep.data, sizeof(HashTable));
    (void)constant;
    ret->hash.hashSet = 0;
    ret->hash.count = 0;
    ret->hash.type = type;
    ret->hash.keyType = keyType;
    ret->hash.elts = 0;
    ret->hash.def = 0;
    Resize(&ret->hash, &hashSets[0]);
    RETURN(ret);
}

/*
 * Make room for a new entry for 'key', which Find placed at '*he'.
 * Growing the table moves the entries, so find the key's place again.
 * Raise out_of_memory when the table is as large as it can get.
 */
static Bool
HashMakeRoom(HashTablePtr ht, HashEltPtr *he, Value hash, Value key)
{
    if (ht->count < ht->hashSet->entries)
        return True;
    if (ht->hashSet == &hashSets[NHASHSETS - 1]) {
        RaiseStandardException(exception_out_of_memory, 1, NewStrString("hash table too large"));
        return False;
    }
    Resize(ht, ht->hashSet + 1);
    *he = Find(ht, hash, key);
    return True;
}

Value
HashGet(Value hv, Value key)
{
    HashTablePtr ht = &hv->hash;
    Value        hash = ValueHash(key);
    HashEltPtr   he;
    Value        value;

    he = Find(ht, hash, key);
    if (!HashEltValid(he)) {
        if (!ht->def) {
            RaiseStandardException(exception_uninitialized_value, 1,
                                   NewStrString("key not in hash"));
            return (Void);
        }
        if (!HashMakeRoom(ht, &he, hash, key))
            return (Void);
        ht->count++;
        he->hash = hash;
        he->key = Copy(key);
        HashEltSetBox(ht, he, Copy(ht->def));
    }
    value = HashEltValue(he);
    if (!value) {
        RaiseStandardException(exception_uninitialized_value, 1,
                               NewStrString("uninitialized hash value"));
        return (Void);
    }
    return value;
}

void
HashSet(Value hv, Value key, Value value)
{
    HashTablePtr ht = &hv->hash;
    Value        hash = ValueHash(key);
    HashEltPtr   he;

    he = Find(ht, hash, key);
    if (!HashEltValid(he)) {
        if (!HashMakeRoom(ht, &he, hash, key))
            return;
        ht->count++;
        he->hash = hash;
        he->key = Copy(key);
        HashEltSetBox(ht, he, Copy(value));
    } else
        BoxValueSet(he->box, 0, Copy(value));
}

void
HashSetDef(Value hv, Value def)
{
    HashTablePtr ht = &hv->hash;

    ht->def = Copy(def);
}

Value
HashRef(Value hv, Value key)
{
    ENTER();
    HashTablePtr ht = &hv->hash;
    HashEltPtr   he;
    Value        hash = ValueHash(key);

    he = Find(ht, hash, key);
    if (!HashEltValid(he)) {
        if (!HashMakeRoom(ht, &he, hash, key))
            RETURN(Void);
        ht->count++;
        he->hash = hash;
        he->key = Copy(key);
        if (ht->def)
            HashEltSetBox(ht, he, Copy(ht->def));
        else
            HashEltSetBox(ht, he, 0);
    }
    RETURN(NewRef(he->box, 0));
}

Value
HashTest(Value hv, Value key)
{
    HashTablePtr ht = &hv->hash;
    HashEltPtr   he;
    Value        hash = ValueHash(key);

    he = Find(ht, hash, key);
    return HashEltValid(he) ? TrueVal : FalseVal;
}

void
HashDelete(Value hv, Value key)
{
    HashTablePtr ht = &hv->hash;
    HashEltPtr   he;
    Value        hash = ValueHash(key);

    he = Find(ht, hash, key);
    if (HashEltValid(he)) {
        /*
         * Mark this entry as deleted: no key, but keeping the box, which
         * references to the value may still hold. A key added here later
         * gets a box of its own
         */
        he->hash = 0;
        he->key = 0;
        --ht->count;
    }
}

Value
HashKeys(Value hv)
{
    ENTER();
    HashTablePtr ht = &hv->hash;
    ptrdiff_t    dim = ht->count;
    Value        keys = NewArray(False, True, ht->keyType, 1, &dim);
    ptrdiff_t    h;
    ptrdiff_t    i = 0;

    for (h = 0; h < ht->elts->size; h++) {
        HashEltPtr e = &ht->elts->elts[h];

        if (HashEltValid(e)) {
            ArrayValueSet(&keys->array, i, e->key);
            i++;
        }
    }
    if (i != dim)
        ArrayResize(keys, 0, i);
    RETURN(keys);
}

Value
HashCopy(Value hv)
{
    ENTER();
    Value     new = NewHash(False, hv->hash.keyType, hv->hash.type);
    ptrdiff_t h;

    new->hash.def = hv->hash.def;
    Resize(&new->hash, hv->hash.hashSet);
    Rehash(hv->hash.elts, &new->hash);
    /*
     * Rehash shared the boxes; give the copy boxes of its own, holding
     * copies of the values as array copies copy their elements
     */
    for (h = 0; h < new->hash.elts->size; h++) {
        HashEltPtr e = &new->hash.elts->elts[h];

        if (HashEltValid(e)) {
            Value v = HashEltValue(e);

            if (v)
                v = Copy(v);
            HashEltSetBox(&new->hash, e, v);
        }
    }
    RETURN(new);
}
