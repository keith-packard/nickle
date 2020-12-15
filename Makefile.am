## Process this file with automake to produce Makefile.in

## Copyright © 1988-2005 Keith Packard and Bart Massey.
## All Rights Reserved.  See the file COPYING in this directory
## for licensing information.

AUTOMAKE_OPTIONS = foreign
SUBDIRS = bench test examples doc

RELEASE_DATE := @RELEASE_DATE@

NICKLEFILES = builtin.5c math.5c scanf.5c mutex.5c \
	arc4.5c prng.5c command.5c abort.5c \
        printf.5c history.5c ctype.5c string.5c socket.5c \
	file.5c parse-args.5c svg.5c process.5c \
	prime_sieve.5c factorial.5c gamma.5c sort.5c list.5c skiplist.5c \
	json.5c

DEBIAN = debian/changelog debian/compat debian/source/format debian/watch \
	 debian/control debian/copyright debian/rules debian/lintian.override \
	 debian/tests/control debian/tests/upstream debian/upstream/signing-key.asc
	 
EXTRA_DIST = README.name README.release autogen.sh ChangeLog \
	$(NICKLEFILES) $(DEBIAN) nickle.1.in nickle.spec.in

MAINTAINERCLEANFILES=ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || \
	(touch ChangeLog; echo 'git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

nickleincludedir=$(includedir)/nickle

nickleinclude_HEADERS = \
	builtin.h nickle-config.h mem.h nickle.h ref.h value.h \
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
	builtin-date.c builtin.c builtin.h \
	builtin-foreign.c gram.y lex.l

nickle_LDFLAGS=$(NICKLE_LDFLAGS)

pkgdata_DATA = $(NICKLEFILES) COPYING

man_MANS = nickle.1

AM_YFLAGS = -d

AM_CPPFLAGS = \
	-DBUILD=\""$(RELEASE_DATE)"\" \
	-DBUILD_VERSION=\""$(VERSION)"\" \
	-DLOCAL_BUILD \
	-DNICKLELIBDIR=\"@nicklelibdir@\"

AM_CFLAGS = \
	-D_FORTIFY_SOURCE=2 \
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

builtin.o main.o: Makefile

TARFILE=$(PACKAGE)-$(VERSION).tar.gz
SIGFILE=$(PACKAGE)-$(VERSION).tar.gz.asc
DEBFILE=$(PACKAGE)_$(VERSION)_amd64.deb
SRPMFILE=$(RPMDIR)/SRPMS/$(PACKAGE)-$(VERSION)-1.src.rpm
RPMFILE=$(RPMDIR)/RPMS/$(PACKAGE)-$(VERSION)-1.x86_64.rpm
RELEASE_FILES = $(TARFILE) $(SIGFILE) $(DEBFILE) $(SRPMFILE) $(RPMFILE)
DEB_TAR_DIR=$(PACKAGE)_$(VERSION).orig
DEB_TAR=$(DEB_TAR_DIR).tar.gz
DEB_SIG=$(DEB_TAR).asc

debuild: $(DEBFILE)

$(DEBFILE): $(DEB_TAR) $(DEB_SIG) $(TARFILE)
	tar xzf $(TARFILE)
	(cd $(distdir) && pdebuild --buildresult $(abs_top_builddir) --auto-debsign)

debuild-unsigned: $(DEB_TAR) $(TARFILE)
	tar xzf $(distdir).tar.gz
	(cd $(distdir)/debian && debuild -us -uc)

$(DEB_TAR): $(TARFILE)
	rm -f $@
	rm -rf $(DEB_TAR_DIR)
	tar xzf $(TARFILE) --transform="s%$(distdir)%$(DEB_TAR_DIR)%"
	rm -rf $(DEB_TAR_DIR)/debian
	tar czf $@ $(DEB_TAR_DIR)

$(DEB_SIG): $(DEB_TAR)
	rm -f $@
	gpg -a -b $(DEB_TAR)

nickle.1: nickle.1.in nickle-config.h
	sed -e 's,@''VERSION@,$(VERSION),' \
	    -e 's,@''BUILD_DATE@,$(BUILD_DATE),' \
	    -e 's,@''pkgdatadir@,$(pkgdatadir),' $(top_srcdir)/nickle.1.in > $@
	    
nickle.spec: nickle.spec.in nickle-config.h
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

$(SIGFILE): $(TARFILE)
	rm -f $@
	gpg -a --detach-sign $(TARFILE)

release-files: $(RELEASE_FILES)

release: $(RELEASE_FILES)
	scp $(RELEASE_FILES) nickle.org:/var/www/nickle/release

.PHONY: debuild debuild-signed debuild-unsigned debuild-dirs rpm force
