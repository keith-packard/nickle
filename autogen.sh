#! /bin/sh
#
# $Id$
#
# runs autotools to create ./configure and friends
#
autoreconf -v --install || exit 1
./configure --enable-maintainer-mode "$@"
