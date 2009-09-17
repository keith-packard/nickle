check_SCRIPTS=gcdtest.5c \
	inttest.5c \
	optest.5c \
	orderofoptest.5c \
	rattest.5c \
	reftest.5c \
	modtest.5c \
	hashtest.5c \
	signal.5c \
	round.5c \
	math.5c

noinst_PROGRAMS=math-tables

TABLES=math-tables.5c

TESTS_ENVIRONMENT=NICKLESTART=$(top_srcdir)/builtin.5c NICKLEPATH=$(top_srcdir) ../nickle
TESTS=$(check_SCRIPTS)

EXTRA_DIST=$(check_SCRIPTS)

$(TABLES): math-tables
	./math-tables > $(TABLES)


check-TESTS: math-tables.5c
