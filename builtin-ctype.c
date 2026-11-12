/*
 * Copyright © 2026 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * Unicode character classes and case mappings, from tables that
 * mkunicode.py generates from the Unicode Character Database
 */

#include "builtin.h"

static NamespacePtr CtypeNamespace;

enum {
    CTYPE_none = 0,
    CTYPE_alnum = 1 << 0,
    CTYPE_alpha = 1 << 1,
    CTYPE_blank = 1 << 2,
    CTYPE_cntrl = 1 << 3,
    CTYPE_digit = 1 << 4,
    CTYPE_graph = 1 << 5,
    CTYPE_lower = 1 << 6,
    CTYPE_print = 1 << 7,
    CTYPE_punct = 1 << 8,
    CTYPE_space = 1 << 9,
    CTYPE_upper = 1 << 10,
    CTYPE_xdigit = 1 << 11,
    /* A letter whose case mapping tells whether it is lower or upper */
    CTYPE_case = 1 << 12,
};

/* Each run lasts until the next one starts */
static const struct {
    uint32_t first;
    uint32_t classes;
} ctype_class[] = {
#include "ctype-class.h"
};

/*
 * Each case run covers first..last:
 *
 *  CASE_TOLO	lower case is c + delta
 *  CASE_TOUP	upper case is c + delta
 *  CASE_TOBOTH	a title case letter: lower case is c + 1, upper c - 1
 *  CASE_TO1	capital and small letters alternate, capitals on even
 *		codes for CASE_EVENCAP and odd codes for CASE_ODDCAP
 */
enum { CASE_TO1, CASE_TOLO, CASE_TOUP, CASE_TOBOTH };
enum { CASE_EVENCAP, CASE_ODDCAP };

static const struct {
    uint32_t first;
    uint32_t last;
    int32_t  mode;
    int32_t  delta;
} ctype_case[] = {
#include "ctype-case.h"
};

#define NUM(a) ((ptrdiff_t)(sizeof(a) / sizeof((a)[0])))

/*
 * Return the case run holding 'c', or -1
 */
static ptrdiff_t
CtypeCaseRun(uint32_t c)
{
    ptrdiff_t lo = 0, hi = NUM(ctype_case) - 1;

    while (lo <= hi) {
        ptrdiff_t mid = (lo + hi) / 2;

        if (c < ctype_case[mid].first)
            hi = mid - 1;
        else if (c > ctype_case[mid].last)
            lo = mid + 1;
        else
            return mid;
    }
    return -1;
}

/*
 * Return whether a capital letter is at 'c' in a CASE_TO1 run
 */
static Bool
CtypeCapital(ptrdiff_t run, uint32_t c)
{
    return (c & 1) == (ctype_case[run].delta == CASE_ODDCAP);
}

/*
 * Return the classes of 'c', resolving CTYPE_case into CTYPE_lower or
 * CTYPE_upper
 */
static uint32_t
CtypeClasses(uint32_t c)
{
    ptrdiff_t lo = 0, hi = NUM(ctype_class) - 1;

    /* Find the last run starting at or before c */
    while (lo < hi) {
        ptrdiff_t mid = (lo + hi + 1) / 2;

        if (ctype_class[mid].first <= c)
            lo = mid;
        else
            hi = mid - 1;
    }

    uint32_t classes = ctype_class[lo].classes;

    if (classes & CTYPE_case) {
        ptrdiff_t run = CtypeCaseRun(c);

        classes &= ~(uint32_t)CTYPE_case;
        if (run >= 0) {
            switch (ctype_case[run].mode) {
            case CASE_TO1:
                if (CtypeCapital(run, c))
                    classes |= CTYPE_upper;
                else
                    classes |= CTYPE_lower;
                break;
            case CASE_TOLO:
                classes |= CTYPE_upper;
                break;
            case CASE_TOUP:
                classes |= CTYPE_lower;
                break;
            case CASE_TOBOTH:
                classes |= CTYPE_upper | CTYPE_lower;
                break;
            }
        }
    }
    return classes;
}

/*
 * Return whether 'av' is a character, setting 'c'. Other integers,
 * such as the -1 getc returns at end of file, are in no class
 */
static Bool
CtypeChar(Value av, uint32_t *c)
{
    ptrdiff_t p;

    if (!ValueToPtrdiff(av, &p) || p < 0 || p > NICKLE_CHAR_MAX)
        return False;
    *c = (uint32_t)p;
    return True;
}

static Value
CtypeIs(Value av, uint32_t class)
{
    uint32_t c;

    if (!CtypeChar(av, &c))
        return FalseVal;
    if (CtypeClasses(c) & class)
        return TrueVal;
    return FalseVal;
}

static Value
do_Ctype_isalnum(Value av)
{
    return CtypeIs(av, CTYPE_alnum);
}

static Value
do_Ctype_isalpha(Value av)
{
    return CtypeIs(av, CTYPE_alpha);
}

static Value
do_Ctype_isblank(Value av)
{
    return CtypeIs(av, CTYPE_blank);
}

static Value
do_Ctype_iscntrl(Value av)
{
    return CtypeIs(av, CTYPE_cntrl);
}

static Value
do_Ctype_isdigit(Value av)
{
    return CtypeIs(av, CTYPE_digit);
}

static Value
do_Ctype_isgraph(Value av)
{
    return CtypeIs(av, CTYPE_graph);
}

static Value
do_Ctype_islower(Value av)
{
    return CtypeIs(av, CTYPE_lower);
}

static Value
do_Ctype_isprint(Value av)
{
    return CtypeIs(av, CTYPE_print);
}

static Value
do_Ctype_ispunct(Value av)
{
    return CtypeIs(av, CTYPE_punct);
}

static Value
do_Ctype_isspace(Value av)
{
    return CtypeIs(av, CTYPE_space);
}

static Value
do_Ctype_isupper(Value av)
{
    return CtypeIs(av, CTYPE_upper);
}

static Value
do_Ctype_isxdigit(Value av)
{
    return CtypeIs(av, CTYPE_xdigit);
}

/*
 * Return the simple case mapping of 'av' toward upper case when
 * 'upper' is set, else toward lower case
 */
static Value
CtypeConvert(Value av, Bool upper)
{
    uint32_t  c;
    ptrdiff_t run;

    if (!CtypeChar(av, &c) || (run = CtypeCaseRun(c)) < 0)
        return av;

    int64_t to = c;

    switch (ctype_case[run].mode) {
    case CASE_TO1:
        if (CtypeCapital(run, c)) {
            if (!upper)
                to = c + 1;
        } else if (upper) {
            to = c - 1;
        }
        break;
    case CASE_TOLO:
        if (!upper)
            to = (int64_t)c + ctype_case[run].delta;
        break;
    case CASE_TOUP:
        if (upper)
            to = (int64_t)c + ctype_case[run].delta;
        break;
    case CASE_TOBOTH:
        if (upper)
            to = c - 1;
        else
            to = c + 1;
        break;
    }
    return NewCharInteger((uint32_t)to);
}

static Value
do_Ctype_tolower(Value av)
{
    return CtypeConvert(av, False);
}

static Value
do_Ctype_toupper(Value av)
{
    return CtypeConvert(av, True);
}

void
import_Ctype_namespace(void)
{
    ENTER();
    static const struct fbuiltin_1 funcs_1[] = {
        { do_Ctype_isalnum, "isalnum", "b", "i",
         "\n"
          " bool isalnum (int c)\n"
          "\n"
          " Return whether 'c' is a letter or an ASCII digit\n" },
        { do_Ctype_isalpha, "isalpha", "b", "i",
         "\n"
          " bool isalpha (int c)\n"
          "\n"
          " Return whether 'c' is alphabetic, or a non-ASCII decimal digit\n" },
        { do_Ctype_isblank, "isblank", "b", "i",
         "\n"
          " bool isblank (int c)\n"
          "\n"
          " Return whether 'c' is a tab or a space other than a no-break space\n" },
        { do_Ctype_iscntrl, "iscntrl", "b", "i",
         "\n"
          " bool iscntrl (int c)\n"
          "\n"
          " Return whether 'c' is a control character or a line or\n"
          " paragraph separator\n" },
        { do_Ctype_isdigit, "isdigit", "b", "i",
         "\n"
          " bool isdigit (int c)\n"
          "\n"
          " Return whether 'c' is an ASCII digit\n" },
        { do_Ctype_isgraph, "isgraph", "b", "i",
         "\n"
          " bool isgraph (int c)\n"
          "\n"
          " Return whether 'c' is a visible character\n" },
        { do_Ctype_islower, "islower", "b", "i",
         "\n"
          " bool islower (int c)\n"
          "\n"
          " Return whether 'c' is lower case\n" },
        { do_Ctype_isprint, "isprint", "b", "i",
         "\n"
          " bool isprint (int c)\n"
          "\n"
          " Return whether 'c' is a visible character or a space\n" },
        { do_Ctype_ispunct, "ispunct", "b", "i",
         "\n"
          " bool ispunct (int c)\n"
          "\n"
          " Return whether 'c' is visible and neither alphabetic nor a digit\n" },
        { do_Ctype_isspace, "isspace", "b", "i",
         "\n"
          " bool isspace (int c)\n"
          "\n"
          " Return whether 'c' is white space\n" },
        { do_Ctype_isupper, "isupper", "b", "i",
         "\n"
          " bool isupper (int c)\n"
          "\n"
          " Return whether 'c' is upper case\n" },
        { do_Ctype_isxdigit, "isxdigit", "b", "i",
         "\n"
          " bool isxdigit (int c)\n"
          "\n"
          " Return whether 'c' is an ASCII hexadecimal digit\n" },
        { do_Ctype_tolower, "tolower", "i", "i",
         "\n"
          " int tolower (int c)\n"
          "\n"
          " Return the lower case of 'c', or 'c' when it has none\n" },
        { do_Ctype_toupper, "toupper", "i", "i",
         "\n"
          " int toupper (int c)\n"
          "\n"
          " Return the upper case of 'c', or 'c' when it has none\n" },
        { 0 }
    };

    CtypeNamespace = BuiltinNamespace(/*parent*/ 0, "Ctype")->namespace.namespace;

    BuiltinFuncs1(&CtypeNamespace, funcs_1);
    EXIT();
}
