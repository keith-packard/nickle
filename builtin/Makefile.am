noinst_LIBRARIES = libbuiltin.a

DEFINES = -DBUILD=\""`sh $(top_builddir)/date-sh`"\"

AM_CFLAGS = -Wall -Wpointer-arith -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations \
	-Wnested-externs $(DEFINES) -I$(top_srcdir)

libbuiltin_a_SOURCES =	builtin.h namespaces.h version.h builtin.c \
			command.c debug.c environ.c \
			file.c math.c semaphore.c \
			string.c thread.c toplevel.c \
			sockets.c # bsdrandom.c gcd.c
