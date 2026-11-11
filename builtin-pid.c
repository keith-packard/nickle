/*
 * Copyright © 1988-2008 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * builtin-process.c
 *
 * provide builtin functions for the PID namespace
 */

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <grp.h>

#include "builtin.h"

static NamespacePtr PIDNamespace;

/*
 * Return user or group id 'id' as a value; ids may exceed an int
 */
static Value
id_value(id_t id)
{
    ENTER();
    RETURN(Reduce(NewInteger(Positive, NewNatural((double_digit)id))));
}

/*
 * Store 'v' in '*idp', raising invalid_argument and returning False when
 * it isn't a user or group id
 */
static Bool
id_part(Value v, char *error, id_t *idp)
{
    ptrdiff_t p;

    if (!ValueToPtrdiff(v, &p) || p < 0 || (ptrdiff_t)(id_t)p != p) {
        RaiseStandardException(exception_invalid_argument, 3, NewStrString(error), NewInt(0), v);
        return False;
    }
    *idp = (id_t)p;
    return True;
}

static Value
do_PID_getuid(void)
{
    ENTER();
    RETURN(id_value(getuid()));
}

static Value
do_PID_geteuid(void)
{
    ENTER();
    RETURN(id_value(geteuid()));
}

static Value
do_PID_getgid(void)
{
    ENTER();
    RETURN(id_value(getgid()));
}

static Value
do_PID_getegid(void)
{
    ENTER();
    RETURN(id_value(getegid()));
}

static Value
error(Value value)
{
    int err = errno;

    RaiseStandardException(exception_system_error, 3, FileGetErrorMessage(err), NewInt(err), value);
    return Void;
}

static Value
do_PID_getgroups(void)
{
    ENTER();
    int       n;
    gid_t    *list;
    Value     ret;
    ptrdiff_t i;

    n = getgroups(0, NULL);
    if (n < 0)
        RETURN(error(Void));
    list = AllocateTemp((size_t)n * sizeof(gid_t));
    if (getgroups(n, list) < 0)
        RETURN(error(NewInt(n)));
    ret = NewArray(False, False, typePrim[rep_integer], 1, &(ptrdiff_t) { n });
    for (i = 0; i < n; i++)
        ArrayValueSet(&ret->array, i, id_value(list[i]));
    RETURN(ret);
}

static Value
do_PID_getpid(void)
{
    ENTER();
    RETURN(NewInt(getpid()));
}

static Value
do_PID_setuid(Value uid)
{
    ENTER();
    id_t u;

    if (!id_part(uid, "Invalid uid", &u))
        RETURN(Void);

    if (setuid((uid_t)u) < 0)
        RETURN(error(uid));

    RETURN(Void);
}

static Value
do_PID_seteuid(Value euid)
{
    ENTER();
    id_t u;

    if (!id_part(euid, "Invalid euid", &u))
        RETURN(Void);

    if (seteuid((uid_t)u) < 0)
        RETURN(error(euid));

    RETURN(Void);
}

static Value
do_PID_setgid(Value gid)
{
    ENTER();
    id_t u;

    if (!id_part(gid, "Invalid gid", &u))
        RETURN(Void);

    if (setgid((gid_t)u) < 0)
        RETURN(error(gid));

    RETURN(Void);
}

static Value
do_PID_setegid(Value egid)
{
    ENTER();
    id_t u;

    if (!id_part(egid, "Invalid egid", &u))
        RETURN(Void);

    if (setegid((gid_t)u) < 0)
        RETURN(error(egid));

    RETURN(Void);
}

static Value
do_PID_setgroups(Value groups)
{
    ENTER();
    ptrdiff_t n;
    ptrdiff_t i;
    gid_t    *g;

    n = ArrayLimits(&groups->array)[0];
    g = AllocateTemp((size_t)n * sizeof(gid_t));
    for (i = 0; i < n; i++) {
        id_t id;

        if (!id_part(ArrayValueGet(&groups->array, i), "Invalid gid", &id))
            RETURN(Void);
        g[i] = (gid_t)id;
    }

    if (setgroups((size_t)n, g) < 0)
        RETURN(error(groups));

    RETURN(Void);
}

void
import_PID_namespace(void)
{
    ENTER();

    static const struct fbuiltin_0 funcs_0[] = {
        { do_PID_getuid, "getuid", "i", "",
         "\n"
          " int getuid ()\n"
          "\n"
          " Return the current uid\n" },
        { do_PID_geteuid, "geteuid", "i", "",
         "\n"
          " int geteuid ()\n"
          "\n"
          " Return the current effective uid\n" },
        { do_PID_getgid, "getgid", "i", "",
         "\n"
          " int getgid ()\n"
          "\n"
          " Return the current gid\n" },
        { do_PID_getegid, "getegid", "i", "",
         "\n"
          " int getegid ()\n"
          "\n"
          " Return the current effective gid\n" },
        { do_PID_getgroups, "getgroups", "Ai", "",
         "\n"
          " int[*] getgroups ()\n"
          "\n"
          " Return the list of additional groups\n" },
        { do_PID_getpid, "getpid", "i", "",
         "\n"
          " int getpid ()\n"
          "\n"
          " Return the current process id." },
        { 0 }
    };
    static const struct fbuiltin_1 funcs_1[] = {
        { do_PID_setuid, "setuid", "v", "i",
         "\n"
          " void setuid (int uid)\n"
          "\n"
          " Set the current uid." },
        { do_PID_seteuid, "seteuid", "v", "i",
         "\n"
          " void seteuid (int euid)\n"
          "\n"
          " Set the current euid." },
        { do_PID_setgid, "setgid", "v", "i",
         "\n"
          " void setgid (int gid)\n"
          "\n"
          " Set the current gid." },
        { do_PID_setegid, "setegid", "v", "i",
         "\n"
          " void setegid (int egid)\n"
          "\n"
          " Set the current egid." },
        { do_PID_setgroups, "setgroups", "v", "Ai",
         "\n"
          " void setgroups (int[*] groups)\n"
          "\n"
          " Set the list of additional groups." },
        { 0 }
    };

    static const struct ebuiltin excepts[] = {
        { "system_error", exception_system_error, "sEp",
         "\n"
          " system_error (string message, error_type error, poly value)\n"
          "\n"
          " Raised when a system function fails.\n"
          " 'message' is a printable error string.\n"
          " 'error' is a symbolic error code.\n"
          " 'value' is the value which failed.\n"          },
        { 0,              0,                      0,     0 },
    };
    const struct ebuiltin *e;

    PIDNamespace = BuiltinNamespace(/*parent*/ 0, "PID")->namespace.namespace;

    for (e = excepts; e->name; e++)
        BuiltinAddException(&PIDNamespace, e->exception, e->name, e->args, e->doc);

    BuiltinFuncs0(&PIDNamespace, funcs_0);
    BuiltinFuncs1(&PIDNamespace, funcs_1);
    EXIT();
}
