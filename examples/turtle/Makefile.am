NICKLEFILES=\
	snowflake.5c \
	turtle.5c

exampledir=$(pkgdatadir)/examples/turtle

example_DATA=$(NICKLEFILES) snowflake.tex

EXTRA_DIST=$(example_DATA)

TESTS_ENVIRONMENT=NICKLESTART=$(top_srcdir)/builtin.5c NICKLEPATH=$(top_srcdir):$(top_srcdir)/$(subdir) $(top_builddir)/nickle
TESTS=snowflake.5c

CLEANFILES=snowflake.eepic snowflake.aux snowflake.dvi snowflake.log

