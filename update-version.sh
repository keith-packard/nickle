#!/bin/sh
FILE=version.m4
CURHEADER="`sed -e '/CURHEADER/!d' -e 's=^.*\$\(.*\)\$$=\1=' <$FILE`"
OLDHEADER="`sed -e '/OLDHEADER/!d' -e 's=^.*<\(.*\)>$=\1=' <$FILE`"
CURVERSION="`sed -e '/CURVERSION/!d' -e 's=^[^0-9]*\(.*\)$=\1=' <$FILE`"

if [ "$CURHEADER" != "$OLDHEADER" ]
then
    CURVERSION="`echo \"$CURVERSION\" |
                 awk -F. '{printf("%d.%d\n", $1, $2+1);}'`"
    OLDHEADER="$CURHEADER"
fi

( echo dnl 'This file is maintained automatically by update-version.sh'
  echo dnl 'The file format is finicky, so do not hand-edit lightly.'
  echo dnl 'Last updated' "`date`"
  echo dnl CURHEADER "\$$CURHEADER\$"
  echo dnl OLDHEADER "<$OLDHEADER>"
  echo dnl CURVERSION $CURVERSION
  echo "define([VERSION_NUMBER],[$CURVERSION])" ) > version.m4.new &&
/bin/mv -f version.m4.new version.m4

( echo '/* This file is maintained automatically by update-version.sh */'
  echo "#define BUILD_VERSION \"$CURVERSION\"" ) > builtin/version.h
