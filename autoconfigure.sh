#!/bin/sh
aclocal
autoheader
autoconf
automake -a -c
echo Running ./configure
./configure --enable-maintainer-mode "$@"
echo 
echo "Now type 'make' to compile $PROJECT."
