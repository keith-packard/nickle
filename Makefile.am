## Process this file with automake to produce Makefile.in

## Copyright (C) 1988-2001 Keith Packard and Bart Massey.
## All Rights Reserved.  See the file COPYING in this directory
## for licensing information.

EXTRA_DIST = README.name

bin_PROGRAMS = nickle

nickle_SOURCES = alarm.c array.c atom.c avl.c box.c builtin.c \
	compile.c debug.c divide.c edit.c \
	error.c execute.c expr.c file.c float.c frame.c func.c \
	gcd.c history.c int.c integer.c io.c main.c mem.c \
	natural.c pretty.c rational.c ref.c refer.c \
	sched.c scope.c stack.c string.c struct.c \
	symbol.c sync.c type.c union.c util.c value.c \
	avl.h mem.h memp.h nickle.h opcode.h ref.h stack.h value.h \
	gram.y lex.l

data_DATA = builtin.5c math.5c scanf.5c mutex.5c arc4.5c prng.5c command.5c

man_MANS = nickle.1

YFLAGS = -d

DEFINES = -DNICKLELIB=\""$(datadir)"\" -DBUILD=\""`$(srcdir)/date-sh`"\"

CDEBUGFLAGS = -O -g

CFLAGS = $(CDEBUGFLAGS) -Wall -Wpointer-arith -Wstrict-prototypes \
-Wmissing-prototypes -Wmissing-declarations \
-Wnested-externs $(DEFINES)
