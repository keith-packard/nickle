check_SCRIPTS=\
	scanf.5c \
	gcdtest.5c \
	inttest.5c \
	optest.5c \
	orderofoptest.5c \
	rattest.5c \
	reftest.5c \
	arraytest.5c \
	modtest.5c \
	hashtest.5c \
	signal.5c \
	round.5c \
	math.5c \
	factorial.5c \
	is_type.5c \
	jsontest.5c \
	datetest.5c \
	string-file.5c \
	sorttest.5c \
	chacha_test.5c

TABLES=math-tables.5c

# Install test files so they can be run later

testdir=$(pkgdatadir)/test

test_DATA=$(check_SCRIPTS) $(TABLES)

CLEANFILES=$(TABLES)

TESTS_ENVIRONMENT=NICKLESTART=$(top_srcdir)/builtin.5c NICKLEPATH=$(top_srcdir) MALLOC_CHECK_=3 MALLOC_PERTURB_=$(shell echo $$(($$RANDOM % 255 + 1)))
TESTS=$(check_SCRIPTS)
LOG_COMPILER=$(top_builddir)/nickle

EXTRA_DIST=$(check_SCRIPTS) math-tables.c

math.5c: $(TABLES)

noinst_PROGRAMS = math-tables

math_tables_SOURCES = math-tables.c

math_tables_LIBS = -lm

$(TABLES): math-tables$(BUILD_EXEEXT)
	./math-tables$(BUILD_EXEEXT) > $(TABLES)
