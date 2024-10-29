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
math_tables_dep = custom_target('math-tables.5c',
				capture: true,
				command: prog_math_tables,
				output: 'math-tables.5c'
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
