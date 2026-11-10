/*
 * Copyright © 2017 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	date.c
 *
 *	provide builtin functions for the Date namespace
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <strings.h>
#include <time.h>
#include "builtin.h"

NamespacePtr DateNamespace;

static Type *typeDate;

#define DATE_I 0
#define DATE_S "00"

static Value
int_value(signed_digit s)
{
    return Reduce(NewSignedDigitInteger(s));
}

/*
 * Return member 'member' of 's' less 'offset', or 'def' less 'offset'
 * when it is missing, raising invalid_argument when the result is
 * outside [min, max]
 */
static long long
value_offset(Value s, Atom member, char *error, ptrdiff_t def, ptrdiff_t offset, long long min,
             long long max)
{
    Value     ref = StructMemRef(s, AtomId(member));
    Value     mem = 0;
    ptrdiff_t p = def;

    if (ref != 0)
        mem = RefValueGet(ref);
    if (mem != 0) {
        p = PtrdiffPart(mem, error);
        if (aborting)
            return 0;
    }
    if ((long long)p < min + offset || (long long)p > max + offset) {
        if (!mem)
            mem = NewPtrdiffInteger(p);
        RaiseStandardException(exception_invalid_argument, 3, NewStrString(error), NewInt(0), mem);
        return 0;
    }
    return (long long)p - offset;
}

static int
value_int(Value s, Atom member, char *error, int def, int offset)
{
    return (int)value_offset(s, member, error, def, offset, INT_MIN, INT_MAX);
}

static int
value_bool(Value s, Atom member, char *error, int def)
{
    Value ref = StructMemRef(s, AtomId(member));
    Value mem;

    if (ref == 0)
        return def;

    mem = RefValueGet(ref);
    if (mem == 0)
        return def;

    return BoolPart(mem, error);
}

#ifdef HAVE_STRUCT_TM_TM_ZONE
static const char *
value_string(Value s, Atom member, char *error, const char *def)
{
    Value ref = StructMemRef(s, AtomId(member));
    Value mem;

    if (ref == 0)
        return def;

    mem = RefValueGet(ref);
    if (mem == 0)
        return def;

    return StrzPart(mem, error);
}
#endif

static Value
to_date(struct tm *tm)
{
    Value  ret;
    BoxPtr box;

    ret = NewStruct(TypeCanon(typeDate)->structs.structs, False);
    box = ret->structs.values;
    BoxValueSet(box, 0, int_value(tm->tm_sec));
    BoxValueSet(box, 1, int_value(tm->tm_min));
    BoxValueSet(box, 2, int_value(tm->tm_hour));
    BoxValueSet(box, 3, int_value(tm->tm_mday));
    BoxValueSet(box, 4, int_value((signed_digit)tm->tm_mon + 1));
    BoxValueSet(box, 5, int_value((signed_digit)tm->tm_year + 1900));
    BoxValueSet(box, 6, int_value(tm->tm_wday));
    BoxValueSet(box, 7, int_value(tm->tm_yday));
    BoxValueSet(box, 8, tm->tm_isdst ? TrueVal : FalseVal);
#ifdef HAVE_STRUCT_TM_TM_GMTOFF
    BoxValueSet(box, 9, int_value(tm->tm_gmtoff));
#else
    BoxValueSet(box, 9, 0);
#endif
#ifdef HAVE_STRUCT_TM_TM_ZONE
    BoxValueSet(box, 10, NewStrString(tm->tm_zone));
#else
    BoxValueSet(box, 10, NewStrString("NONE"));
#endif
    return ret;
}

static void
from_date(Value date, struct tm *tm)
{
    tm->tm_sec = value_int(date, "sec", "invalid sec", 0, 0);
    tm->tm_min = value_int(date, "min", "invalid min", 0, 0);
    tm->tm_hour = value_int(date, "hour", "invalid hour", 0, 0);
    tm->tm_mday = value_int(date, "mday", "invalid mday", 1, 0);
    tm->tm_mon = value_int(date, "mon", "invalid mon", 1, 1);
    tm->tm_year = value_int(date, "year", "invalid year", 1970, 1900);
    tm->tm_wday = value_int(date, "wday", "invalid wday", 0, 0);
    tm->tm_yday = value_int(date, "yday", "invalid yday", 0, 0);
    tm->tm_isdst = value_bool(date, "isdst", "invalid isdst", 0);
#ifdef HAVE_STRUCT_TM_TM_GMTOFF
    tm->tm_gmtoff = (long)value_offset(date, "gmtoff", "invalid gmtoff", 0, 0, LONG_MIN, LONG_MAX);
#endif
#ifdef HAVE_STRUCT_TM_TM_ZONE
    tm->tm_zone = (char *)value_string(date, "zone", "invalid zone", "NONE");
#endif
}

/*
 * Store 'v' in '*seconds', raising invalid_argument and returning
 * False when it isn't a time_t
 */
static Bool
time_part(Value v, time_t *seconds)
{
    signed_digit s = SignedDigitPart(v, "Illegal time");

    if (aborting)
        return False;
    if ((time_t)s != s) {
        RaiseStandardException(exception_invalid_argument, 3, NewStrString("Illegal time"),
                               NewInt(0), v);
        return False;
    }
    *seconds = (time_t)s;
    return True;
}

static Value
do_Date_gmtime(Value v)
{
    ENTER();
    time_t    seconds;
    struct tm result;

    if (!time_part(v, &seconds))
        RETURN(Void);
    /* gmtime_r fails when the year doesn't fit in an int */
    if (!gmtime_r(&seconds, &result)) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("gmtime: time out of range"), NewInt(0), v);
        RETURN(Void);
    }
    RETURN(to_date(&result));
}

static Value
do_Date_localtime(Value v)
{
    ENTER();
    time_t    seconds;
    struct tm result;

    if (!time_part(v, &seconds))
        RETURN(Void);
    /* localtime_r fails when the year doesn't fit in an int */
    if (!localtime_r(&seconds, &result)) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("localtime: time out of range"), NewInt(0), v);
        RETURN(Void);
    }
    RETURN(to_date(&result));
}

static Value
do_Date_timegm(Value v)
{
    ENTER();
    struct tm tm;
    time_t    seconds;

    from_date(v, &tm);
    if (aborting)
        RETURN(Void);
    /* -1 is also a valid time, so only errno tells failure apart */
    errno = 0;
    seconds = timegm(&tm);
    if (seconds == (time_t)-1 && errno != 0) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("timegm: date out of range"), NewInt(0), v);
        RETURN(Void);
    }
    RETURN(Reduce(NewSignedDigitInteger((signed_digit)seconds)));
}

static Value
do_Date_timelocal(Value v)
{
    ENTER();
    struct tm tm;
    time_t    seconds;

    from_date(v, &tm);
    if (aborting)
        RETURN(Void);
    /* -1 is also a valid time, so only errno tells failure apart */
    errno = 0;
    seconds = mktime(&tm);
    if (seconds == (time_t)-1 && errno != 0) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("timelocal: date out of range"), NewInt(0), v);
        RETURN(Void);
    }
    RETURN(Reduce(NewSignedDigitInteger((signed_digit)seconds)));
}

static Type *
make_typedef(char *name_str, Namespace *namespace, Publish publish, int usertype_id, Symbol **sret,
             Type *type)
{
    ENTER();
    Atom    name = AtomId(name_str);
    Symbol *sym = NewSymbolType(name, type);
    Type   *typed = NewTypeName(NewExprAtom(name, 0, False), sym);

    NamespaceAddName(namespace, sym, publish);

    BuiltinSetUserdefType(typed, usertype_id);
    MemAddRoot(typed);
    if (sret)
        *sret = sym;
    RETURN(typed);
}

void
import_Date_namespace(void)
{
    ENTER();
    static const struct fbuiltin_1 funcs_1[] = {
        { do_Date_gmtime, "gmtime", DATE_S, "i",
         "\n"
          " date_t gmtime (int time)\n"
          "\n"
          " Convert 'time' into a date_t structure using UTC.\n" },
        { do_Date_localtime, "localtime", DATE_S, "i",
         "\n"
          " date_t localtime (int time)\n"
          "\n"
          " Convert 'time' into a date_t structure using the local timezone.\n" },
        { do_Date_timegm, "timegm", "i", DATE_S,
         "\n"
          " int timegm (date_t date)\n"
          "\n"
          " Convert 'date' into seconds using UTC.\n" },
        { do_Date_timelocal, "timelocal", "i", DATE_S,
         "\n"
          " int timelocal (date_t date)\n"
          "\n"
          " Convert 'date' into seconds using the local timezone.\n" },
        { do_Date_timelocal, "mktime", "i", DATE_S,
         "\n"
          " int mktime (date_t date)\n"
          "\n"
          " Convert 'date' into seconds using the local timezone.\n" },
        { 0 }
    };

    DateNamespace = BuiltinNamespace(/*parent*/ 0, "Date")->namespace.namespace;

    typeDate
        = make_typedef("date_t", DateNamespace, publish_public, DATE_I, NULL,
                       BuildStructType(11, typePrim[rep_integer], "sec", typePrim[rep_integer],
                                       "min", typePrim[rep_integer], "hour", typePrim[rep_integer],
                                       "mday", typePrim[rep_integer], "mon", typePrim[rep_integer],
                                       "year", typePrim[rep_integer], "wday", typePrim[rep_integer],
                                       "yday", typePrim[rep_bool], "isdst", typePrim[rep_integer],
                                       "gmtoff", typePrim[rep_string], "zone"));

    BuiltinFuncs1(&DateNamespace, funcs_1);
    EXIT();
}
