#! /bin/sh
#
# $Id$
#
# runs autotools to create ./configure and friends
#

PROJECT=nickle

srcdir=`dirname "$0"`
test -z $srcdir && srcdir=.

origdir=`pwd`
cd "$srcdir"

if test -z "$AUTOGEN_SUBDIR_MODE" && test -z "$*"
then
  echo "I am going to run ./configure with no arguments - if you wish "
  echo "to pass any to it, please specify them on the $0 command line."
fi

# Use the versioned executables if available.
aclocal=aclocal-1.7
automake=automake-1.7
$aclocal  --version </dev/null >/dev/null 2>&1 || aclocal=aclocal
$automake --version </dev/null >/dev/null 2>&1 || automake=automake

# require libtool >= 1.5
libtool_version=`libtool --version | head -1 | awk '{print $4}'`
libtool_major=`echo $libtool_version | awk -F . '{print $1}'`
libtool_minor=`echo $libtool_version | awk -F . '{print $2}'`
if [ $libtool_major -lt 1 ] || [ $libtool_major -eq 1 -a $libtool_minor -lt 5 ]
then
    echo "libtool 1.5 or later is required. You have $libtool_version."
    exit 1
fi

rm -f config.guess config.sub depcomp install-sh missing mkinstalldirs
rm -f config.cache acconfig.h
rm -rf autom4te.cache

set_option=':'
test -n "${BASH_VERSION+set}" && set_option='set'

$set_option -x

$aclocal $ACLOCAL_FLAGS                  || exit 1
libtoolize --force --copy                || exit 1
autoheader                               || exit 1
$automake --foreign --add-missing --copy || exit 1
autoconf                                 || exit 1
cd "$origdir"                            || exit 1

if test -z "$AUTOGEN_SUBDIR_MODE"
then
  "$srcdir/configure" "$@" || exit 1
  $set_option +x
fi

exit 0

