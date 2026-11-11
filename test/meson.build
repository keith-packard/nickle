#
# Copyright © 1988-2024 Keith Packard and Bart Massey.
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.
#

tests = [
  'scanf.5c',
  'gcdtest.5c',
  'bitwisetest.5c',
  'remaindertest.5c',
  'imprecisetest.5c',
  'realstringtest.5c',
  'floatprinttest.5c',
  'hyperbolictest.5c',
  'erftest.5c',
  'intmathtest.5c',
  'powtest.5c',
  'complexfunctest.5c',
  'exceptionargstest.5c',
  'constfoldtest.5c',
  'noncalltest.5c',
  'hugeinttest.5c',
  'implicitdimtest.5c',
  'lvaluetest.5c',
  'catcherrortest.5c',
  'emptycomptest.5c',
  'bigarraytest.5c',
  'unionmembertest.5c',
  'recursivetypetest.5c',
  'hashuninittest.5c',
  'killtest.5c',
  'stringindextest.5c',
  'hashdefaulttest.5c',
  'karatsubatest.5c',
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
  'fround.5c',
  'sqrttest.5c',
  'cbrttest.5c',
  'sincostest.5c',
  'tantest.5c',
  'exptest.5c',
  'logtest.5c',
  'etest.5c',
  'gammatest.5c',
  'ldexptest.5c',
  'complextest.5c',
  'jntest.5c',
  'yntest.5c',
  'factorial.5c',
  'is_type.5c',
  'jsontest.5c',
  'datetest.5c',
  'string-file.5c',
  'filtertest.5c',
  'byteargtest.5c',
  'sorttest.5c',
  'chacha_test.5c',
  'manylocalstest.5c',
  'floatprectest.5c',
  'semcounttest.5c',
  'datelimittest.5c',
  'reopentest.5c',
  'openmodetest.5c',
  'millistest.5c',
  'charrangetest.5c',
  'utf8leadtest.5c',
  'highfdtest.5c',
  'vwidthtest.5c',
  'socketaddrtest.5c',
  'socketconnecttest.5c',
  'threadprioritytest.5c',
  'bignamespacetest.5c',
  'longlisttest.5c',
  'commandreleasetest.5c'
]

extra_files = [
  'math-bits.5c'
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

# Errors in input named on the command line, and unhandled exceptions,
# must exit with an error status
error_tests = [
  'syntax-error.5c',
  'compile-error.5c',
  'unhandled-if.5c',
  'unhandled-try.5c',
]

foreach test : error_tests
  test(test, nickle,
       args: files(test),
       env: test_env,
       should_fail: true
      )
endforeach

test('expr-error', nickle,
     args: ['-e', 'int x = ;', '-e', 'exit (0);'],
     env: test_env,
     should_fail: true
    )

install_data(files(tests + error_tests + extra_files), install_dir: nickle_testdir)
