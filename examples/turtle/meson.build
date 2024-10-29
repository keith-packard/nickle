#
# Draw "snowflake" fractal
#
# Copyright © 2001  Bart Massey.
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.
nickle_files =[ 'snowflake.5c', 'turtle.5c']

turtle_dir = nickle_examplesdir / 'turtle'

install_data(nickle_files + ['snowflake.tex', 'COPYING'],
	     install_dir: turtle_dir)

test('snowflake', nickle,
     args: files('snowflake.5c'),
     timeout: 240,
     env: test_env)

