
SGMLFILES=nickle-tutorial.sgml
PDFFILES=$(SGMLFILES:.sgml=.pdf)

.sgml.pdf:
	docbook2pdf $<

SGMLINC= \
	tour/tour.sgml \
	basics/invoke.sgml \
	basics/command.sgml \
	intro/variables.sgml \
	intro/expressions.sgml \
	intro/statements.sgml \
	intro/functions.sgml \
	builtins/io.sgml \
	builtins/math.sgml \
	builtins/strings.sgml \
	advanced/copying.sgml \
	advanced/namespaces.sgml \
	advanced/exceptions.sgml \
	advanced/concurrency.sgml \
	advanced/continuations.sgml

EXTRA_DIST=$(SGMLFILES) $(SGMLINC)

if HASDOCBOOK

all-local: $(PDFFILES)

clean-local:
	$(RM) -f $(PDFFILES)


$(PDFFILES): $(SGMLINC)

doc_DATA = $(PDFFILES)

endif