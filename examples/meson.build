#
# Copyright © 1988-2024 Keith Packard and Bart Massey.
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.
#

examples = [
  'comb.5c', 'cribbage.5c', 'erat.5c', 'fourfours.5c',
  'initializer.5c', 'is-prime.5c', 'kaiser.5c', 'menace2.5c',
  'miller-rabin.5c', 'mutextest.5c', 'numbers.5c', 'polynomial.5c',
  'prime.5c', 'qbrating.5c', 'randtest.5c', 'restart.5c', 'roman.5c',
  'rsa-demo.5c', 'rsa.5c', 'skiplisttest.5c', 'sudoku.5c'
]

install_data(examples + ['COPYING'],
	     install_dir: nickle_examplesdir)

# Examples not installed, which are formatted too
examples_uninstalled = [
  'apsp.5c', 'circle.5c', 'coroutines.5c', 'crc.5c', 'google-puzzle.5c',
  'mplex.5c', 'rijndael.5c', 'search.5c', 'shortpaths.5c'
]

nickle_format_files += files(examples + examples_uninstalled)

subdir('smlng')
subdir('turtle')
