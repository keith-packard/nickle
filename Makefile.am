## Process this file with automake to produce Makefile.in

## Copyright © 1988-2004 Keith Packard and Bart Massey.
## All Rights Reserved.  See the file COPYING in this directory
## for licensing information.

SUBDIRS = bench test

VERSION = $(shell sh $(top_builddir)/version-sh $(top_srcdir)/ChangeLog)
BUILD_DATE = $(shell sh $(top_builddir)/date-sh)

NICKLEFILES = builtin.5c math.5c scanf.5c mutex.5c \
	arc4.5c prng.5c command.5c abort.5c \
        printf.5c history.5c ctype.5c string.5c socket.5c \
	file.5c parse-args.5c
EXAMPLES = examples/circle.5c examples/comb.5c examples/kaiser.5c \
	examples/miller-rabin.5c examples/numbers.5c examples/prime.5c \
	examples/qbrating.5c examples/roman.5c examples/rsa-demo.5c \
	examples/rsa.5c
TEST = test/gcdtest.5c test/inttest.5c test/rattest.5c

DEBIAN = debian/nickle.install debian/changelog debian/compat \
	 debian/control debian/copyright debian/rules
	 
EXTRA_DIST = README.name autogen.sh update-version.sh make-version.sh \
	$(NICKLEFILES) gram.h $(EXAMPLES) $(TEST) $(DEBIAN) ChangeLog \
	nickle.spec.in

bin_PROGRAMS = nickle

nickle_SOURCES = \
	alarm.c array.c atom.c avl.c box.c compile.c debug.c \
	divide.c edit.c error.c execute.c expr.c file.c float.c \
	frame.c func.c gcd.c hash.c int.c integer.c io.c main.c \
	mem.c natural.c pretty.c profile.c rational.c ref.c \
	refer.c sched.c scope.c stack.c string.c struct.c \
	symbol.c sync.c type.c union.c util.c value.c avl.h \
	mem.h memp.h nickle.h opcode.h ref.h stack.h value.h \
	builtin-command.c builtin-debug.c builtin-environ.c \
	builtin-file.c builtin-math.c builtin-namespaces.h \
	builtin-semaphore.c builtin-sockets.c builtin-string.c \
	builtin-thread.c builtin-toplevel.c builtin.c builtin.h \
	gram.y lex.l

pkgdata_DATA = $(NICKLEFILES)

man_MANS = nickle.1

AM_YFLAGS = -d

AM_CPPFLAGS = \
	-DBUILD=\""$(BUILD_DATE)"\" \
	-DBUILD_VERSION=\""$(VERSION)"\"

AM_CFLAGS = \
	-Wall -Wpointer-arith -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations \
	-Wnested-externs -fno-strict-aliasing

USES_GRAM_H = \
	compile.o \
	error.o \
	expr.o \
	file.o \
	lex.o \
	main.o \
	pretty.o \
	type.o

$(USES_GRAM_H): gram.h

builtin.o: ChangeLog

debuild debuild-signed: debuild-dirs
	(cd $(distdir)/debian && debuild)

debuild-unsigned: debuild-dirs
	(cd $(distdir)/debian && debuild -us -uc)

debuild-dirs: distdir
	rm -f $(PACKAGE)_$(VERSION).orig.tar.gz
	rm -rf $(distdir).orig
	cp -a $(distdir) $(distdir).orig
	rm -rf $(distdir).orig/debian

#
# This assumes you've got Mike Harris's rpmbuild-nonroot stuff installed
# using the defaults
#
rpm: dist-gzip
	mkdir -p $(HOME)/rpmbuild/$(PACKAGE)-$(VERSION)
	cp $(PACKAGE)-$(VERSION).tar.gz $(HOME)/rpmbuild/$(PACKAGE)-$(VERSION)
	rpm -ba nickle.spec

.PHONY: debuild debuild-signed debuild-unsigned debuild-dirs rpm

