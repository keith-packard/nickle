ADOCFILES=nickle-tutorial.adoc
PDFFILES=$(ADOCFILES:.adoc=.pdf)

PDF_DATE=$(shell date -ud '$(RELEASE_DATE)')

.adoc.pdf:
	asciidoctor-pdf -o $@ $<

ADOCINC= \
	tour/tour.adoc \
	basics/invoke.adoc \
	basics/command.adoc \
	intro/variables.adoc \
	intro/expressions.adoc \
	intro/statements.adoc \
	intro/functions.adoc \
	builtins/io.adoc \
	builtins/math.adoc \
	builtins/strings.adoc \
	advanced/copying.adoc \
	advanced/namespaces.adoc \
	advanced/exceptions.adoc \
	advanced/concurrency.adoc \
	advanced/continuations.adoc

IMAGES = nickle_obverse.jpg

EXTRA_DIST=$(ADOCFILES) $(ADOCINC) $(IMAGES)

if HASASCIIDOCTORPDF

all-local: $(PDFFILES)

clean-local:
	$(RM) -f $(PDFFILES)


$(PDFFILES): $(ADOCINC)

doc_DATA = $(PDFFILES)

endif
