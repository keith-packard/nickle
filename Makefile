# $Header$
#
# makefile for nick
#

#
# configure these per site -- BINDIR is where the executable
# lines, MANDIR is where the manual lives. Make sure MANEXT
# is appropriate
#

ROOT=/local
BINDIR=$(ROOT)/bin
MANEXT=1
MANDIR=$(ROOT)/man/man$(MANEXT)
VERSION=1.1.0

LEX=flex -I
YACC=yacc -d

# for most machines
CMACHINE=

# for Sparc -- otherwise gcc will use functions for mul/div
#CMACHINE=-mv8

CC=gcc -Wall -Wstrict-prototypes $(CMACHINE)

CDEBUGFLAGS=-O2
CFLAGS=$(CDEBUGFLAGS)
BUILDOPTS=-DVERSION='"$(VERSION)"' -DBUILD='"'"`date +%D`"'"'

OFILES= gram.o lex.o \
	alarm.o array.o atom.o avl.o box.o builtin.o \
	compile.o debug.o divide.o double.o edit.o \
	error.o execute.o expr.o file.o frame.o func.o \
	gcd.o history.o int.o integer.o io.o main.o mem.o \
	natural.o pretty.o rational.o ref.o refer.o \
	sched.o scope.o stack.o string.o struct.o \
	symbol.o sync.o util.o value.o	
	
CSOURCE=alarm.c array.c atom.c avl.c box.c builtin.c \
	compile.c debug.c divide.c double.c edit.c \
	error.c execute.c expr.c file.c frame.c func.c \
	gcd.c history.c int.c integer.c io.c main.c mem.c \
	natural.c pretty.c rational.c ref.c refer.c \
	sched.c scope.c stack.c string.c struct.c \
	symbol.c sync.c util.c value.c
	
HSOURCE=avl.h mem.h memp.h nick.h opcode.h ref.h stack.h value.h
ASOURCE=gram.y lex.l
CINTER=gram.c lex.c
HINTER=y.tab.h
CFILES=$(CINTER) $(CSOURCE)
SOURCE=$(HSOURCE) $(ASOURCE) $(CSOURCE)
MANUAL=$(MANDIR)/nick.$(MANEXT)
DOC=nick.1 README
BIN=$(BINDIR)/nick
DIST=Makefile $(SOURCE) $(DOC)

nick: $(OFILES) $(LIBS)
	$(CC) $(CFLAGS) -o nick $(OFILES) $(LIBS) -lm

tags: $(SOURCE)
	ctags -w $(SOURCE)

install: $(BIN) $(MANUAL)

saber: $(CSOURCE) $(CINTER)
	#load $(CSOURCE) $(CINTER)
$(BIN): nick
	install -c -s -m 775 nick $(BIN)

$(MANUAL): nick.1
	install -c -m 664 nick.1 $(MANUAL)

nick.shar: $(DIST)
	shar $(DIST) > nick.shar
	
nick.tar: $(DIST)
	tar cf $@ $(DIST)

nick.tar.Z: $(DIST)
	tar cf - $(DIST) | compress > $@

nick.tar.gz: $(DIST)
	tar czf $@ $(DIST)

nick.uu: $(DIST)
	tar cf - $(DIST) | compress | uuencode nick.tar.Z > $@
	
rcs_tag: $(DIST)
	echo `echo V_$(VERSION) | sed 's/\./_/g'` $(DIST)

clean:
	rm -f $(OFILES) $(CINTER) $(HINTER) nick
	
lint: $(CFILES)
	lint $(CFILES) > lint

ci:
	ci -u $(DIST)

echo:
	@echo $(DIST)

gram.c: gram.y
	$(YACC) gram.y
	mv y.tab.c gram.c

lex.c: lex.l
	$(LEX) lex.l
	mv lex.yy.c lex.c
	
IFILES=nick.h opcode.h value.h mem.h stack.h
$(OFILES): $(IFILES)
expr.o pretty.o type.o lex.o compile.o: y.tab.h
mem.o: avl.h memp.h
builtin.o: Makefile builtin.c
	$(CC) $(CFLAGS) $(BUILDOPTS) -c builtin.c
