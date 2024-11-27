#
# Copyright © 1988-2024 Keith Packard and Bart Massey.
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.
#

tests = [
  'scanf.5c',
  'gcdtest.5c',
  'inttest.5c',
  'optest.5c',
  'orderofoptest.5c',
  'rattest.5c',
  'reftest.5c',
  'arraytest.5c',
  'modtest.5c',
  'hashtest.5c',
  'signal.5c',
  'round.5c',
  'math.5c',
  'factorial.5c',
  'is_type.5c',
  'jsontest.5c',
  'datetest.5c',
  'string-file.5c',
  'sorttest.5c',
  'chacha_test.5c'
]

prog_math_tables = find_program('math-tables.sh', required: true)
prog_bc = find_program('bc', required: true)
prog_fmt = find_program('fmt', required: true)
prog_tr = find_program('tr', required: true)
prog_expr = find_program('expr', required: true)

math_tables_dep = custom_target('math-tables.5c',
				command: [prog_math_tables, '-o', '@OUTPUT@'],
				output: 'math-tables.5c',
				env: {
				       'BC': prog_bc.full_path(),
				       'EXPR': prog_expr.full_path(),
				       'FMT': prog_fmt.full_path(),
				       'TR': prog_tr.full_path()
				     },
				depend_files: 'math-funcs.bc',
				install: true,
				install_dir: nickle_testdir
			       )

foreach test : tests
  test(test, nickle,
       args: files(test),
       depends: math_tables_dep,
       env: test_env,
       workdir: meson.current_build_dir(),
       timeout: 1000
      )
endforeach

install_data(tests, install_dir: nickle_testdir)
