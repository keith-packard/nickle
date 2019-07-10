#! /bin/sh
#
# $Id$
#
# runs autotools to create ./configure and friends
#
# configure depends on version.m4, but autoreconf does not realize this
rm configure
autoreconf -Wall -v --install || exit 1
./configure "$@"
