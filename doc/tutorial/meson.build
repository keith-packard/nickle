#
# Copyright © 1988-2024 Keith Packard and Bart Massey.
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.
#

prog_asciidoctor = find_program('asciidoctor-pdf', required: false)

adoc_files = [
  'advanced/namespaces.adoc',
  'advanced/continuations.adoc',
  'advanced/copying.adoc',
  'advanced/concurrency.adoc',
  'advanced/exceptions.adoc',
  'nickle-tutorial.adoc',
  'builtins/math.adoc',
  'builtins/strings.adoc',
  'builtins/io.adoc',
  'basics/command.adoc',
  'basics/invoke.adoc',
  'tour/tour.adoc',
  'intro/functions.adoc',
  'intro/variables.adoc',
  'intro/statements.adoc',
  'intro/expressions.adoc',
]

if prog_asciidoctor.found()
  custom_target('nickle-tutorial',
		input : 'nickle-tutorial.adoc',
		output: 'nickle-tutorial.pdf',
		command: [ prog_asciidoctor, '-o', '@OUTPUT@', '@INPUT@' ],
		depend_files: files(adoc_files),
		install: true,
		install_dir: nickle_docdir)
endif
