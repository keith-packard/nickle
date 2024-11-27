#! /bin/sh
#
# $Id$
#
# runs autotools to create ./configure and friends
#
# configure depends on version.m4, but autoreconf does not realize this
dir=`dirname $0`
rm $dir/configure
(cd $dir && autoreconf -Wall -v --install) || exit 1
$dir/configure "$@"
