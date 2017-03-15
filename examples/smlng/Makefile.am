#
# SGML-like parser
#
# Copyright © 2001 Keith Packard and Carl Worth
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.
#
NICKLEFILES=\
	context.5c \
	generate.5c \
	parse.5c \
	test.5c

exampledir=$(pkgdatadir)/examples/smlng

example_DATA=$(NICKLEFILES) COPYING

TESTS_ENVIRONMENT=NICKLESTART=$(top_srcdir)/builtin.5c NICKLEPATH=$(top_srcdir):$(top_srcdir)/$(subdir)
LOG_COMPILER=$(top_builddir)/nickle < $(top_srcdir)/$(subdir)/data.sgml

TESTS=test.5c

EXTRA_DIST=$(NICKLEFILES) data.sgml COPYING
