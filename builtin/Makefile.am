noinst_LIBRARIES = libbuiltin.a

libbuiltin_a_SOURCES = builtin.h builtin.c command.c debug.c environ.c \
	file.c history.c math.c semaphore.c string.c thread.c toplevel.c # bsdrandom.c gcd.c

builtin.c: ../gram.h
../gram.h:
	$(MAKE) -C .. gram.h
