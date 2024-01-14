CFILES=choose.c \
	composite.c \
	ifact.c \
	rfact.c
BCFILES=choose.bc \
	composite.bc \
	ifact.bc \
	rfact.bc
NICKLEFILES = choose.5c \
	composite.5c \
	ifact.5c \
	rfact.5c
HARNESS=runbench.sh

CLEANFILES=choose composite ifact rfact

benchdir=$(pkgdatadir)/bench

bench_DATA=$(CFILES) $(BCFILES) $(NICKLEFILES)

bench_SCRIPTS=$(HARNESS)

EXTRA_DIST=$(bench_DATA) $(bench_SCRIPTS)
