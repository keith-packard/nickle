noinst_LIBRARIES = libbuiltin.a

DEFINES = -DBUILD=\""`$(top_srcdir)/date-sh`"\"

CFLAGS = $(CDEBUGFLAGS) -Wall -Wpointer-arith -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations \
	-Wnested-externs $(DEFINES)

libbuiltin_a_SOURCES =	builtin.h namespaces.h builtin.c \
			command.c debug.c environ.c \
			file.c history.c math.c semaphore.c \
			string.c thread.c toplevel.c \
			# bsdrandom.c gcd.c

builtin.c: ../gram.h

../gram.h:
	$(MAKE) -C .. gram.h

$(libbuiltin_a_OBJECTS): builtin.h namespaces.h
