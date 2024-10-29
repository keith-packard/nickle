#
# Copyright © 1988-2024 Keith Packard and Bart Massey.
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.
#

benchmarks = ['choose', 'composite', 'ifact', 'rfact']

gmp_dep = cc.find_library('gmp')

bc = find_program('bc', required: false)

foreach benchmark : benchmarks

  benchmark('c-' + benchmark,
	    executable('c-' + benchmark, benchmark + '.c',
		       dependencies: gmp_dep))

  if bc.found()
    benchmark('bc-' + benchmark, bc,
	      args: files([benchmark + '.bc', 'quit.bc']))
  endif

  benchmark('nickle-' + benchmark, nickle,
	    args: files(benchmark + '.5c'),
	    env: test_env)

endforeach
