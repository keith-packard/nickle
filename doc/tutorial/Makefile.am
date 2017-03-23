
SGMLFILES=nickle-tutorial.sgml
PDFFILES=$(SGMLFILES:.sgml=.pdf)

PDF_DATE=$(shell date -ud '$(RELEASE_DATE)')

.sgml.pdf:
	docbook2tex $< && \
	pdfjadetex '\pdfinfo{/CreationDate($(PDF_DATE))/ModDate($(PDF_DATE))}\input{$*.tex}' >/dev/null && \
	pdfjadetex '\pdfinfo{/CreationDate($(PDF_DATE))/ModDate($(PDF_DATE))}\input{$*.tex}' >/dev/null && \
	pdfjadetex '\pdfinfo{/CreationDate($(PDF_DATE))/ModDate($(PDF_DATE))}\input{$*.tex}' >/dev/null && \
	sed -i '/^\/ID \[<[0-9A-F][0-9A-F]*> <[0-9A-F][0-9A-F]*>\]/d' $@ && \
	rm $*.aux $*.log *$.out *$.tex

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
