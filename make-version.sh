#!/bin/sh
FILE=$1
VERSION="`sed -e '/^version [0-9.][0-9.]*$/!d' \"$FILE\" | sed -e '1!d' -e 's/^.* //'`"

( echo "/* This file is created automatically using $FILE by make-version.sh */"
  echo "#define BUILD_VERSION \"$VERSION\"" ) > version.h
