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

#include	<ctype.h>
#include	<strings.h>
#include	<time.h>
#include	"builtin.h"

NamespacePtr DateNamespace;

static Type *typeDate;

#define DATE_I	0
#define DATE_S	"00"

static Value
int_value(int s)
{
    return Reduce(NewSignedDigitInteger((signed_digit) s));
}

static int
value_int(Value s, Atom member, char *error, int def)
{
    Value	ref = StructMemRef(s, AtomId(member));
    Value	mem;

    if (ref == 0)
	return def;

    mem = RefValueGet(ref);
    if (mem == 0)
	return def;
    return IntPart(mem, error);
}

static int
value_bool(Value s, Atom member, char *error, int def)
{
    Value	ref = StructMemRef(s, AtomId(member));
    Value	mem;

    if (ref == 0)
	return def;

    mem = RefValueGet(ref);
    if (mem == 0)
	return def;

    return BoolPart(mem, error);
}

static Value
to_date(struct tm *tm)
{
    Value ret;
    BoxPtr	box;

    ret = NewStruct(TypeCanon(typeDate)->structs.structs, False);
    box = ret->structs.values;
    BoxValueSet (box, 0, int_value(tm->tm_sec));
    BoxValueSet (box, 1, int_value(tm->tm_min));
    BoxValueSet (box, 2, int_value(tm->tm_hour));
    BoxValueSet (box, 3, int_value(tm->tm_mday));
    BoxValueSet (box, 4, int_value(tm->tm_mon + 1));
    BoxValueSet (box, 5, int_value(tm->tm_year + 1900));
    BoxValueSet (box, 6, int_value(tm->tm_wday));
    BoxValueSet (box, 7, int_value(tm->tm_yday));
    BoxValueSet (box, 8, tm->tm_isdst ? TrueVal : FalseVal);
    BoxValueSet (box, 9, NewStrString(tm->tm_zone));
    return ret;
}

static void
from_date(Value date, struct tm *tm)
{
    tm->tm_sec = value_int(date, "sec", "invalid sec", 0);
    tm->tm_min = value_int(date, "min", "invalid min", 0);
    tm->tm_hour = value_int(date, "hour", "invalid hour", 0);
    tm->tm_mday = value_int(date, "mday", "invalid mday", 1);
    tm->tm_mon = value_int(date, "mon", "invalid mon", 1) - 1;
    tm->tm_year = value_int(date, "year", "invalid year", 1970) - 1900;
    tm->tm_wday = value_int(date, "wday", "invalid wday", 0);
    tm->tm_yday = value_int(date, "yday", "invalid yday", 0);
    tm->tm_isdst = value_bool(date, "isdst", "invalid isdst", 0);
    tm->tm_zone = NULL;
}

static Value
do_Date_gmtime(Value v)
{
    ENTER();
    time_t	seconds = SignedDigitPart(v, "Illegal time");
    struct tm	result;

    if (aborting)
	RETURN(Void);

    gmtime_r(&seconds, &result);
    RETURN(to_date(&result));
}

static Value
do_Date_localtime(Value v)
{
    ENTER();
    time_t	seconds = SignedDigitPart(v, "Illegal time");
    struct tm	result;

    if (aborting)
	RETURN(Void);

    localtime_r(&seconds, &result);
    RETURN(to_date(&result));
}

static Value
do_Date_timegm(Value v)
{
    ENTER();
    struct tm	tm;
    time_t	seconds;

    from_date(v, &tm);
    seconds = timegm(&tm);
    RETURN(Reduce(NewSignedDigitInteger((signed_digit) seconds)));
}

static Value
do_Date_timelocal(Value v)
{
    ENTER();
    struct tm	tm;
    time_t	seconds;

    from_date(v, &tm);
    seconds = mktime(&tm);
    RETURN(Reduce(NewSignedDigitInteger((signed_digit) seconds)));
}

static Type *
make_typedef (char	*name_str,
	      Namespace	*namespace,
	      Publish	publish,
	      int	usertype_id,
	      Symbol	**sret,
	      Type	*type)
{
    ENTER ();
    Atom    name = AtomId (name_str);
    Symbol  *sym = NewSymbolType (name, type);
    Type    *typed = NewTypeName (NewExprAtom (name, 0, False),
				  sym);

    NamespaceAddName (namespace, sym, publish);

    BuiltinSetUserdefType (typed, usertype_id);
    MemAddRoot (typed);
    if (sret)
	*sret = sym;
    RETURN (typed);
}

void
import_Date_namespace()
{
    ENTER ();
    static const struct fbuiltin_1 funcs_1[] = {
        { do_Date_gmtime, "gmtime", DATE_S, "i", "\n"
	    " date_t gmtime (int time)\n"
	    "\n"
	    " Convert 'time' into a date_t structure using UTC.\n" },
	{ do_Date_localtime, "localtime", DATE_S, "i", "\n"
	    " date_t localtime (int time)\n"
	    "\n"
	    " Convert 'time' into a date_t structure using the local timezone.\n" },
	{ do_Date_timegm, "timegm", "i", DATE_S, "\n"
	    " int timegm (date_t date)\n"
	    "\n"
	    " Convert 'date' into seconds using UTC.\n" },
	{ do_Date_timelocal, "timelocal", "i", DATE_S, "\n"
	    " int timelocal (date_t date)\n"
	    "\n"
	    " Convert 'date' into seconds using the local timezone.\n" },
	{ do_Date_timelocal, "mktime", "i", DATE_S, "\n"
	    " int mktime (date_t date)\n"
	    "\n"
	    " Convert 'date' into seconds using the local timezone.\n" },
        { 0 }
    };

    DateNamespace = BuiltinNamespace (/*parent*/ 0, "Date")->namespace.namespace;

    typeDate = make_typedef("date_t",
			    DateNamespace,
			    publish_public,
			    DATE_I,
			    NULL,
			    BuildStructType (10,
					     typePrim[rep_integer], "sec",
					     typePrim[rep_integer], "min",
					     typePrim[rep_integer], "hour",
					     typePrim[rep_integer], "mday",
					     typePrim[rep_integer], "mon",
					     typePrim[rep_integer], "year",
					     typePrim[rep_integer], "wday",
					     typePrim[rep_integer], "yday",
					     typePrim[rep_bool], "isdst",
					     typePrim[rep_string], "zone"));

    BuiltinFuncs1 (&DateNamespace, funcs_1);
    EXIT ();
}
