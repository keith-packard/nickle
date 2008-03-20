## Process this file with automake to produce Makefile.in

## Copyright © 1988-2005 Keith Packard and Bart Massey.
## All Rights Reserved.  See the file COPYING in this directory
## for licensing information.

AUTOMAKE_OPTIONS = foreign
SUBDIRS = bench test examples

BUILD_DATE := $(shell sh $(top_builddir)/date-sh)

NICKLEFILES = builtin.5c math.5c scanf.5c mutex.5c \
	arc4.5c prng.5c command.5c abort.5c \
        printf.5c history.5c ctype.5c string.5c socket.5c \
	file.5c parse-args.5c svg.5c process.5c

DEBIAN = debian/nickle.install debian/changelog debian/compat \
	 debian/control debian/copyright debian/rules debian/lintian.override
	 
EXTRA_DIST = README.name autogen.sh ChangeLog \
	$(NICKLEFILES) $(DEBIAN) nickle.1.in nickle.spec.in

MAINTAINERCLEANFILES=ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(GIT_DIR=$(top_srcdir)/.git git-log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || \
	(touch ChangeLog; echo 'git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

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
	builtin-thread.c builtin-toplevel.c builtin-pid.c \
	builtin.c builtin.h \
	builtin-foreign.c gram.y lex.l

nickle_LDFLAGS=$(NICKLE_LDFLAGS)

pkgdata_DATA = $(NICKLEFILES) COPYING

man_MANS = nickle.1

AM_YFLAGS = -d

AM_CPPFLAGS = \
	-DBUILD=\""$(BUILD_DATE)"\" \
	-DBUILD_VERSION=\""$(VERSION)"\" \
	-DLOCAL_BUILD \
	-DNICKLELIBDIR=\"@nicklelibdir@\"

AM_CFLAGS = \
	-Wall -Wpointer-arith -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations \
	-Wnested-externs -fno-strict-aliasing -fwrapv

USES_GRAM_H = \
	array.o \
	compile.o \
	error.o \
	expr.o \
	file.o \
	lex.o \
	main.o \
	pretty.o \
	type.o

$(USES_GRAM_H): gram.h

YACCCOMPILE = $(YACC) $(YFLAGS) $(AM_YFLAGS) gram.y && sed -i 's/^short yy/static const short yy/' y.tab.c && echo

builtin.o main.o: Makefile

TARFILE=$(PACKAGE)-$(VERSION).tar.gz
DEBFILE=$(PACKAGE)_$(VERSION)-1_i386.deb
SRPMFILE=$(RPMDIR)/SRPMS/$(PACKAGE)-$(VERSION)-1.src.rpm
RPMFILE=$(RPMDIR)/RPMS/$(PACKAGE)-$(VERSION)-1.i386.rpm
RELEASE_FILES = $(TARFILE) $(DEBFILE) $(SRPMFILE) $(RPMFILE)
DEB_TAR_DIR=$(PACKAGE)_$(VERSION).orig
DEB_TAR=$(DEB_TAR_DIR).tar.gz

debuild: $(DEBFILE)

$(DEBFILE): $(DEB_TAR) $(TARFILE)
	tar xzf $(TARFILE)
	(cd $(distdir)/debian && debuild)

debuild-unsigned: $(DEB_TAR) $(TARFILE)
	tar xzf $(distdir).tar.gz
	(cd $(distdir)/debian && debuild -us -uc)

$(DEB_TAR): $(TARFILE)
	rm -f $(DEB_TAR)
	rm -rf $(DEB_TAR_DIR)
	tar xzf $(TARFILE)
	mv $(distdir) $(DEB_TAR_DIR)
	tar czf $(DEB_TAR) $(DEB_TAR_DIR)

nickle.1: nickle.1.in config.h
	sed -e 's,@''VERSION@,$(VERSION),' \
	    -e 's,@''BUILD_DATE@,$(BUILD_DATE),' \
	    -e 's,@''pkgdatadir@,$(pkgdatadir),' $(top_srcdir)/nickle.1.in > $@
	    
nickle.spec: nickle.spec.in config.h
	sed -e 's,@''VERSION@,$(VERSION),' \
	    -e 's,@''pkgdatadir@,$(pkgdatadir),' $(top_srcdir)/nickle.spec.in > $@
	    
clean-local:
	rm -f nickle.1 nickle.spec

#
# This assumes you've got Mike Harris's rpmbuild-nonroot stuff installed
# using the defaults
#
RPMDIR=$(HOME)/rpmbuild

rpm: $(RPMFILE) $(SRPMFILE)

$(RPMFILE): $(TARFILE) nickle.spec
	mkdir -p $(RPMDIR)/$(PACKAGE)-$(VERSION)
	cp $(TARFILE) $(RPMDIR)/$(PACKAGE)-$(VERSION)
	rpmbuild -ba nickle.spec

$(SRPMFILE): $(RPMFILE)

$(TARFILE): dist-gzip $(DISTFILES)
	touch $(TARFILE)
	echo $(TARFILE) ready

release-files: $(RELEASE_FILES)

release: $(RELEASE_FILES)
	scp $(RELEASE_FILES) nickle.org:/var/www/nickle/release

.PHONY: debuild debuild-signed debuild-unsigned debuild-dirs rpm force
