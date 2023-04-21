dnl Process this file with autoconf to produce a configure script.

dnl Copyright © 1988-2004 Keith Packard and Bart Massey.
dnl All Rights Reserved.  See the file COPYING in this directory
dnl for licensing information.

AC_PREREQ([2.69])

AC_INIT([nickle],[2.91],[http://nickle.org],[nickle])
RELEASE_DATE="2021-08-18"
AC_CONFIG_SRCDIR([nickle.h])
AC_CONFIG_HEADERS([nickle-config.h])
AC_CONFIG_AUX_DIR(.)

AM_INIT_AUTOMAKE([foreign])

AC_SUBST([RELEASE_DATE])
dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AM_PROG_LEX
dnl AC_PROG_YACC
AC_CHECK_PROGS([YACC], byacc yacc bison, yacc)
case "$YACC" in
*bison)	YACC="$YACC -y"	;;
esac

dnl Checks for libraries.
AC_CHECK_FUNC(log,,AC_CHECK_LIB(m, log))
AC_CHECK_FUNC(gethostbyname,,AC_CHECK_LIB(nsl, gethostbyname))
AC_CHECK_FUNC(socket,,AC_CHECK_LIB(socket, socket)
		      AC_CHECK_LIB(resolv, hstrerror))
AC_CHECK_LIB(dl, dlopen)

dnl as-compiler-flag.m4 0.0.1
dnl autostars m4 macro for detection of compiler flags
dnl
dnl ds@schleef.org

AC_DEFUN([AS_COMPILER_FLAG],
[
  AC_MSG_CHECKING([to see if compiler understands $1])

  save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $1"

  AC_LINK_IFELSE([AC_LANG_PROGRAM([],[return 0])], [flag_ok=yes], [flag_ok=no])
    CFLAGS="$save_CFLAGS"

  if test "X$flag_ok" = Xyes; then
      $2
      true
   else
      $3
      true
   fi
   AC_MSG_RESULT([$flag_ok])
])

AS_COMPILER_FLAG([-Wl,-E], GCC_EXTERN="yes", GCC_EXTERN="no")
if test $GCC_EXTERN = yes; then
	LDFLAGS="-Wl,-E"
	AC_DEFINE([HAVE_EXTERN_SYMS], 1, [C compilers can extern program symbols])
else
	LDFLAGS=""
fi

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h strings.h time.h sys/time.h unistd.h sys/resource.h)
AC_CHECK_HEADERS(stropts.h)
AC_CHECK_HEADERS(dlfcn.h)

dnl Checks for precise integer types
AC_CHECK_HEADERS(stdint.h)
case "$ac_cv_header_stdint_h" in
 no)
   AC_CHECK_SIZEOF([unsigned long long], 0)
   AC_CHECK_SIZEOF([unsigned long], 0)
   AC_CHECK_SIZEOF([unsigned int], 0)
   AC_CHECK_SIZEOF([unsigned short], 0)
   ;;
 yes)
   AC_MSG_CHECKING([for uint64_t])
   AC_EGREP_HEADER([uint64_t], stdint.h, 
   		    AC_MSG_RESULT(yes)
		    AC_DEFINE([HAVE_UINT64_T], 1, [Has uint64_t datatype]),
		    AC_MSG_RESULT(no))
   ;;
esac

dnl Checks for library functions.

AC_FUNC_VPRINTF
AC_CHECK_FUNCS(unsetenv setenv putenv gettimeofday hstrerror select)
AC_CHECK_FUNCS(sigaction sigrelse sigignore setrlimit getrlimit)
AC_CHECK_FUNCS(dlopen dlsym dlerror dlclose)
AC_CHECK_FUNC(significand,,AC_CHECK_LIB(m, significand))
AC_CHECK_MEMBER([struct tm.tm_zone],
		AC_DEFINE([HAVE_STRUCT_TM_TM_ZONE], 1, [Has tm_zone in struct tm]),,
		[[#include <time.h>]])
AC_CHECK_MEMBER([struct tm.tm_gmtoff],
		AC_DEFINE([HAVE_STRUCT_TM_TM_GMTOFF], 1, [Has tm_gmtoff in struct tm]),,
		[[#include <time.h>]])

case "$ac_cv_func_significand""$ac_cv_lib_m_significand" in
    *yes*)
	AC_DEFINE(HAVE_SIGNIFICAND,1,[Has significand function])
	;;
esac

AC_PATH_PROGS([ASCIIDOCTORPDF],[asciidoctor-pdf])

AM_CONDITIONAL(HASASCIIDOCTORPDF,test -n "$ASCIIDOCTORPDF")

AC_FUNC_GETPGRP

dnl Handle large files
AC_SYS_LARGEFILE

AC_ARG_WITH(libedit, "use libedit for command line editing", [use_libedit=yes], [use_libedit=no])

case "$use_libedit" in
 yes)
   PKG_CHECK_MODULES([LIBEDIT], [libedit])
   have_libreadline=yes
   LIBREADLINE_CFLAGS="${LIBEDIT_CFLAGS}"
   LIBREADLINE_LIBS="${LIBEDIT_LIBS}"
   READLINE_H="<readline.h>"
   HISTORY_H="<history.h>"
   ;;
 *)
   AC_LIB_READLINE
   echo "done checking for readline"
   if test "x$ac_cv_header_readline_readline_h" = xyes; then
       have_libreadline=yes
       READLINE_H="<readline/readline.h>"
       HISTORY_H="<readline/history.h>"
    fi
   ;;
esac

if test "x$have_libreadline" = "xyes"; then

    AC_DEFINE(HAVE_LIBREADLINE, 1, [Has readline library])
    AC_DEFINE_UNQUOTED(READLINE_H, ${READLINE_H}, [readline.h include path])
    AC_DEFINE_UNQUOTED(HISTORY_H, ${HISTORY_H}, [history.h include path])

    CFLAGS="${LIBREADLINE_CFLAGS} ${CFLAGS}"
    LIBS="${LIBREADLINE_LIBS} ${LIBS}"

    AC_CHECK_DECL(rl_catch_signals,
		  AC_DEFINE(HAVE_RL_CATCH_SIGNALS,1,[Has rl_catch_signals]),,[#include ${READLINE_H}])
    AC_CHECK_DECL(rl_reset_after_signal,
		  AC_DEFINE(HAVE_RL_RESET_AFTER_SIGNAL,1,[Has rl_reset_after_signal]),,[#include ${READLINE_H}])
    AC_CHECK_DECL(rl_cleanup_after_signal,
		  AC_DEFINE(HAVE_RL_CLEANUP_AFTER_SIGNAL,1,[Has rl_cleanup_after_signal]),,[#include ${READLINE_H}])
    AC_CHECK_DECL(rl_echo_signal_char,
		  AC_DEFINE(HAVE_RL_ECHO_SIGNAL_CHAR,1,[Has rl_echo_signal_char]),,[#include ${READLINE_H}])
    AC_CHECK_DECL(rl_free_line_state,
		  AC_DEFINE(HAVE_RL_FREE_LINE_STATE,1,[Has rl_free_line_state]),,[#include ${READLINE_H}])
fi

if test "x$prefix" = xNONE; then
    prefix="$ac_default_prefix"
fi

nicklelibdir=`eval echo ${datadir}`/nickle

AC_SUBST(nicklelibdir)

AC_CONFIG_FILES(
 Makefile
 test/Makefile
 bench/Makefile
 examples/Makefile
 examples/smlng/Makefile
 examples/turtle/Makefile
 doc/Makefile
 doc/tutorial/Makefile)

AC_OUTPUT
