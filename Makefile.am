## Process this file with automake to produce Makefile.in

## Copyright (C) 1988-2001 Keith Packard and Bart Massey.
## All Rights Reserved.  See the file COPYING in this directory
## for licensing information.

EXTRA_DIST = README.name gram.h

bin_PROGRAMS = nickle

nickle_SOURCES = alarm.c array.c atom.c avl.c box.c builtin.c \
	compile.c debug.c divide.c edit.c \
	error.c execute.c expr.c file.c float.c frame.c func.c \
	gcd.c history.c int.c integer.c io.c main.c mem.c \
	natural.c pretty.c rational.c ref.c refer.c \
	sched.c scope.c stack.c string.c struct.c \
	symbol.c sync.c type.c util.c value.c \
	avl.h mem.h memp.h nickle.h opcode.h ref.h stack.h value.h \
	gram.y lex.l

data_DATA = builtin.5c math.5c scanf.5c

man_MANS = nickle.1

YFLAGS = -d

CFLAGS = -O -g -Wall -Wpointer-arith -Wstrict-prototypes \
-Wmissing-prototypes -Wmissing-declarations \
-Wnested-externs-Wall

builtin.o: builtin.c config.status $(srcdir)/date-sh
	$(CC) -c -DBUILD=\""`$(srcdir)/date-sh`"\" -I. -I$(srcdir) $(CPPFLAGS) $(CFLAGS) builtin.c

main.o: main.c
	$(CC) -c -DNICKLELIB=\""$(datadir)/builtin.5c"\" -I. -I$(srcdir) $(CPPFLAGS) $(CFLAGS) main.c

lex.o compile.o expr.o pretty.o: gram.h
