dnl Check for readline and get termcap lib as well
AC_DEFUN([AC_LIB_READLINE],[
  doit=yes
  AC_ARG_WITH(readline,
  [  --with-readline=DIR     GNU readline library in DIR],
  doit=$with_readline)
  case $doit in
  no)
    ;;
  *)
    readline_header="readline"
    readline_libdir=""
    case $doit in
    yes)
      ;;
    *)
      readline_libdir="-L$doit/lib -R$doit/lib"
      readline_incdir="-I$doit/include"
      ;;
    esac
    AC_CHECK_LIB(ncurses,tparm,TERMLIB=-lncurses,
      AC_CHECK_LIB(termcap,tgetent,TERMLIB=-ltermcap))
    saved_LIBS="$LIBS"
    LIBS="$LIBS $TERMLIB"
    saved_LDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS $readline_libdir"
    saved_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $readline_incdir"
    AC_CHECK_LIB(readline,readline,
      [ AC_CHECK_HEADER(readline/readline.h,
          LIBS="$saved_LIBS -lreadline $TERMLIB"
          AC_DEFINE(HAVE_LIBREADLINE,1,
            [support fancy command line editing])) ],
      [ AC_MSG_RESULT([Cannot find readline.  Build with --without-readline or set the readline path appropriately.])
        exit 1 ])
      ;;
  esac
])
