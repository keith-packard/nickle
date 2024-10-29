#
# SGML-like parser
# Copyright © 2001 Keith Packard and Carl Worth
#
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.
#

nickle_files = [ 'context.5c', 'generate.5c', 'parse.5c', 'test.5c' ]

smlng_dir = nickle_examplesdir / 'smlng'

install_data(nickle_files + ['COPYING', 'data.sgml'],
	     install_dir: smlng_dir)

test('smlng', nickle,
     args: files(['test.5c', 'data.sgml']),
     env: test_env,
     timeout: 1000)
