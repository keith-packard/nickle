## Process this file with automake to produce Makefile.in

## Copyright © 1988-2004 Keith Packard and Bart Massey.
## All Rights Reserved.  See the file COPYING in this directory
## for licensing information.

SUBDIRS = bench test examples

VERSION = $(shell sh $(top_builddir)/version-sh $(top_srcdir)/ChangeLog)
BUILD_DATE = $(shell sh $(top_builddir)/date-sh)

NICKLEFILES = builtin.5c math.5c scanf.5c mutex.5c \
	arc4.5c prng.5c command.5c abort.5c \
        printf.5c history.5c ctype.5c string.5c socket.5c \
	file.5c parse-args.5c svg.5c

DEBIAN = debian/nickle.install debian/changelog debian/compat \
	 debian/control debian/copyright debian/rules debian/lintian.override
	 
EXTRA_DIST = README.name autogen.sh \
	$(NICKLEFILES) $(DEBIAN) ChangeLog nickle.1.in nickle.spec.in

nickleincludedir=$(includedir)/nickle

nickleinclude_HEADERS = \
	builtin.h config.h mem.h nickle.h ref.h value.h \
	builtin-namespaces.h gram.h memp.h opcode.h stack.h

bin_PROGRAMS = nickle

nickle_SOURCES = \
	alarm.c array.c atom.c box.c compile.c debug.c \
	divide.c edit.c error.c execute.c expr.c file.c float.c \
	foreign.c frame.c func.c gcd.c hash.c int.c integer.c io.c \
	main.c mem.c natural.c pretty.c profile.c rational.c ref.c \
	refer.c sched.c scope.c stack.c string.c struct.c \
	symbol.c sync.c type.c union.c util.c value.c \
	mem.h memp.h nickle.h opcode.h ref.h stack.h value.h \
	builtin-command.c builtin-debug.c builtin-environ.c \
	builtin-file.c builtin-math.c builtin-namespaces.h \
	builtin-semaphore.c builtin-sockets.c builtin-string.c \
	builtin-thread.c builtin-toplevel.c builtin.c builtin.h \
	builtin-foreign.c gram.y lex.l

nickle_LDFLAGS=$(NICKLE_LDFLAGS)

pkgdata_DATA = $(NICKLEFILES) COPYING

man_MANS = nickle.1

AM_YFLAGS = -d

AM_CPPFLAGS = \
	-DBUILD=\""$(BUILD_DATE)"\" \
	-DBUILD_VERSION=\""$(VERSION)"\" \
	-DLOCAL_BUILD

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

builtin.o: $(nickle_SOURCES)

debuild debuild-signed: debuild-dirs
	(cd $(distdir)/debian && debuild)

debuild-unsigned: debuild-dirs
	(cd $(distdir)/debian && debuild -us -uc)

debuild-dirs: distdir
	rm -f $(PACKAGE)_$(VERSION).orig.tar.gz
	rm -rf $(distdir).orig
	cp -a $(distdir) $(distdir).orig
	rm -rf $(distdir).orig/debian

nickle.1: nickle.1.in ChangeLog config.h
	sed -e 's,@''VERSION@,$(VERSION),' \
	    -e 's,@''BUILD_DATE@,$(BUILD_DATE),' \
	    -e 's,@''pkgdatadir@,$(pkgdatadir),' $(top_srcdir)/nickle.1.in > $@
	    
nickle.spec: nickle.spec.in ChangeLog config.h
	sed -e 's,@''VERSION@,$(VERSION),' \
	    -e 's,@''pkgdatadir@,$(pkgdatadir),' $(top_srcdir)/nickle.spec.in > $@
	    
clean-local:
	rm -f nickle.1 nickle.spec

#
# This assumes you've got Mike Harris's rpmbuild-nonroot stuff installed
# using the defaults
#
RPMDIR=$(HOME)/rpmbuild

rpm: dist-gzip nickle.spec
	mkdir -p $(RPMDIR)/$(PACKAGE)-$(VERSION)
	cp $(PACKAGE)-$(VERSION).tar.gz $(RPMDIR)/$(PACKAGE)-$(VERSION)
	rpm -ba nickle.spec

RELEASE_FILES = \
	$(PACKAGE)-$(VERSION).tar.gz \
	$(PACKAGE)_$(VERSION)-1_i386.deb \
	$(RPMDIR)/SRPMS/$(PACKAGE)-$(VERSION)-1.src.rpm \
	$(RPMDIR)/RPMS/$(PACKAGE)-$(VERSION)-1.i386.rpm

release: $(RELEASE_FILES)
	scp $(RELEASE_FILES) nickle.org:/var/www/nickle/release

.PHONY: debuild debuild-signed debuild-unsigned debuild-dirs rpm force
