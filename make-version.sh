#!/bin/sh
FILE=$1
CURHEADER="`sed -e '/CURHEADER/!d' -e 's=^.*\$\(.*\)\$$=\1=' <$FILE`"
OLDHEADER="`sed -e '/OLDHEADER/!d' -e 's=^.*<\(.*\)>$=\1=' <$FILE`"
CURVERSION="`sed -e '/CURVERSION/!d' -e 's=^[^0-9]*\(.*\)$=\1=' <$FILE`"

( echo '/* This file is maintained automatically by update-version.sh */'
  echo "#define BUILD_VERSION \"$CURVERSION\"" ) > version.h
