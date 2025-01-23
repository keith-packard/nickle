#
# Copyright © 1988-2024 Keith Packard and Bart Massey.
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.
#

project('nickle', 'c',
	default_options: [
	  'buildtype=debugoptimized',
	  'c_std=gnu23',
	  'warning_level=2',
	],
	license : 'BSD',
	meson_version : '>= 1.0',
	version: '2.102'
       )

release_date = '2024-12-24'

prefix = get_option('prefix')
include = prefix / get_option('includedir')
datadir = prefix / get_option('datadir')
docdir = datadir / 'doc'

nickle_libdir = datadir / 'nickle'
nickle_includedir = include / 'nickle'
nickle_docdir = docdir / 'nickle'
nickle_examplesdir = nickle_libdir / 'examples'
nickle_testdir = nickle_libdir / 'test'

cc = meson.get_compiler('c')

add_project_arguments(
  '-DBUILD="@0@"'.format(release_date),
  '-DBUILD_VERSION="@0@"'.format(meson.project_version()),
  '-DLOCAL_BUILD',
  '-DNICKLELIBDIR="@0@"'.format(nickle_libdir),
  language : 'c'
)

add_project_arguments(cc.get_supported_arguments(
  [
    '-fno-strict-aliasing',
    '-fwrapv',
    '-Werror=vla',
    '-Warray-bounds',
    '-Werror=double-promotion',
    '-Wno-missing-braces',
    '-Wno-return-type',
    '-Wno-unused-command-line-argument',
    '-Wmissing-prototypes',
    '-Wmissing-declarations',
    '-Werror=implicit-fallthrough=5',
    '-Werror=unreachable-code-fallthrough',
    '-Werror=implicit-function-declaration',
    '-Wold-style-definition',
    '-Wno-implicit-int',
    '-Wformat',
    '-Wformat-security',
    '-Wdate-time',
    '-Wunused-parameter',
    '-Wmissing-field-initializers',
    '-Wsign-compare',
    '-Wenum-conversion',
    '-Wold-style-declaration',
  ]),
		      language: 'c')

link_args = cc.get_supported_link_arguments('-Wl,-E')
have_extern_syms = link_args != []

if get_option('libedit')
  readline_dep = dependency('libedit')
  readline_h = '<readline.h>'
  history_h = ''
else
  readline_dep = dependency('readline', required: false)
  readline_h = '<readline/readline.h>'
  history_h = '<readline/history.h>'
endif
math_dep = cc.find_library('m', required : true)

getpgrp_void = cc.compiles('''#include <unistd.h>
int func() { return (int) getpgrp(); }
''')

have_dlclose = cc.has_function('dlclose')
have_dlerror = cc.has_function('dlerror')
have_dlfcn_h = cc.has_header('dlfcn.h')
have_dlopen = cc.has_function('dlopen')
have_dlsym = cc.has_function('dlsym')
have_fcntl_h = cc.has_header('fcntl.h')
have_getrlimit = cc.has_function('getrlimit')
have_gettimeofday = cc.has_function('gettimeofday')
have_putenv = cc.has_function('putenv')
have_rl_catch_signals = cc.has_function('rl_catch_signals',
				       prefix: '#include ' + readline_h,
				       dependencies: readline_dep)

have_rl_cleanup_after_signal = cc.has_function('rl_cleanup_after_signal',
				       prefix: '#include ' + readline_h,
				       dependencies: readline_dep)

have_rl_echo_signal_char = cc.has_function('rl_echo_signal_char',
				       prefix: '#include ' + readline_h,
				       dependencies: readline_dep)

have_rl_free_line_state = cc.has_function('rl_free_line_state',
				       prefix: '#include ' + readline_h,
				       dependencies: readline_dep)

have_rl_reset_after_signal = cc.has_function('rl_reset_after_signal',
				       prefix: '#include ' + readline_h,
				       dependencies: readline_dep)
have_setenv = cc.has_function('setenv')
have_setrlimit = cc.has_function('setrlimit')
have_sigaction = cc.has_function('sigaction')
have_sigignore = cc.has_function('sigignore')
have_significand = cc.has_function('significand',
				   prefix: '#include <math.h>',
				   dependencies: math_dep)
have_sigrelse = cc.has_function('sigrelse')
have_stdint_h = cc.has_header('stdint.h')
have_stdint_h = cc.has_header('stdint.h')
have_strings_h = cc.has_header('strings.h')
have_string_h = cc.has_header ('string.h')
have_stropts_h = cc.has_header('stropts.h')
have_struct_tm_tm_gmtoff = cc.has_member('struct tm', 'tm_gmtoff',
					 prefix: '#include <time.h>')
have_struct_tm_tm_zone = cc.has_member('struct tm', 'tm_zone',
					 prefix: '#include <time.h>')
have_sys_resource_h = cc.has_header('sys/resource.h')
have_sys_time_h = cc.has_header('sys/time.h')
have_sys_types_h = cc.has_header('sys/types.h')
have_time_h = cc.has_header('time.h')
have_uint64_t = cc.has_type('uint64_t',
			    prefix: '#include <stdint.h>')
have_unistd_h = cc.has_header('unistd.h')
have_unsetenv = cc.has_function('unsetenv')
have_vfprintf = cc.has_function('vfprintf',
				prefix: '#include <stdio.h>')
sizeof_unsigned_int = cc.sizeof('unsigned int')
sizeof_unsigned_long = cc.sizeof('unsigned long')
sizeof_unsigned_long_long = cc.sizeof('unsigned long long')
sizeof_unsigned_short = cc.sizeof('unsigned short')

conf_data = configuration_data({
				 'GETPGRP_VOID' : getpgrp_void,
				 'HAVE_DLCLOSE' : have_dlclose,
				 'HAVE_DLERROR' : have_dlerror,
				 'HAVE_DLFCN_H' : have_dlfcn_h,
				 'HAVE_DLOPEN' : have_dlopen,
				 'HAVE_DLSYM' : have_dlsym,
				 'HAVE_EXTERN_SYMS' : have_extern_syms,
				 'HAVE_FCNTL_H' : have_fcntl_h,
				 'HAVE_GETRLIMIT' : have_getrlimit,
				 'HAVE_GETTIMEOFDAY' : have_gettimeofday,
				 'HAVE_LIBREADLINE' : readline_dep.found(),
				 'HAVE_PUTENV': have_putenv,
				 'HAVE_RL_CATCH_SIGNALS': have_rl_catch_signals,
				 'HAVE_RL_CLEANUP_AFTER_SIGNAL': have_rl_cleanup_after_signal,
				 'HAVE_RL_ECHO_SIGNAL_CHAR': have_rl_echo_signal_char,
				 'HAVE_RL_FREE_LINE_STATE': have_rl_free_line_state,
				 'HAVE_RL_RESET_AFTER_SIGNAL': have_rl_reset_after_signal,
				 'HAVE_SETENV': have_setenv,
				 'HAVE_SETRLIMIT': have_setrlimit,
				 'HAVE_SIGACTION': have_sigaction,
				 'HAVE_SIGIGNORE': have_sigignore,
				 'HAVE_SIGNIFICAND': have_significand,
				 'HAVE_SIGRELSE': have_sigrelse,
				 'HAVE_STDINT_H': have_stdint_h,
				 'HAVE_STRINGS_H': have_strings_h,
				 'HAVE_STRING_H': have_string_h,
				 'HAVE_STROPTS_H': have_stropts_h,
				 'HAVE_STRUCT_TM_TM_GMTOFF': have_struct_tm_tm_gmtoff,
				 'HAVE_STRUCT_TM_TM_ZONE': have_struct_tm_tm_zone,
				 'HAVE_SYS_RESOURCE_H': have_sys_resource_h,
				 'HAVE_SYS_TIME_H': have_sys_time_h,
				 'HAVE_SYS_TYPES_H': have_sys_types_h,
				 'HAVE_TIME_H': have_time_h,
				 'HAVE_UINT64_T': have_uint64_t,
				 'HAVE_UNISTD_H': have_unistd_h,
				 'HAVE_UNSETENV': have_unsetenv,
				 'HAVE_VPRINTF': have_vfprintf,
				 'PACKAGE_BUGREPORT': '"http://nickle.org"',
				 'READLINE_H': readline_h,
				 'SIZEOF_UNSIGNED_INT': sizeof_unsigned_int,
				 'SIZEOF_UNSIGNED_LONG': sizeof_unsigned_long,
				 'SIZEOF_UNSIGNED_LONG_LONG': sizeof_unsigned_long_long,
				 'SIZEOF_UNSIGNED_SHORT': sizeof_unsigned_short,
				 'VERSION': '"@0@"'.format(meson.project_version()),
				 'pkgdatadir': nickle_libdir,
				 'RELEASE_DATE': release_date,
			       })

if history_h != ''
  conf_data.set('HISTORY_H', history_h)
endif

nickle_srcs = [
  'alarm.c', 'array.c', 'atom.c', 'box.c', 'compile.c', 'debug.c',
  'divide.c', 'edit.c', 'error.c', 'execute.c', 'expr.c', 'file.c',
  'float.c', 'foreign.c', 'frame.c', 'func.c', 'gcd.c', 'hash.c',
  'int.c', 'integer.c', 'io.c', 'main.c', 'mem.c', 'natural.c',
  'pretty.c', 'profile.c', 'rational.c', 'ref.c', 'refer.c',
  'sched.c', 'scope.c', 'stack.c', 'string.c', 'struct.c', 'symbol.c',
  'sync.c', 'type.c', 'union.c', 'util.c', 'value.c', 'mem.h',
  'memp.h', 'nickle.h', 'opcode.h', 'ref.h', 'stack.h', 'value.h',
  'builtin-command.c', 'builtin-debug.c', 'builtin-environ.c',
  'builtin-file.c', 'builtin-math.c', 'builtin-namespaces.h',
  'builtin-semaphore.c', 'builtin-sockets.c', 'builtin-string.c',
  'builtin-thread.c', 'builtin-toplevel.c', 'builtin-pid.c',
  'builtin-date.c', 'builtin.c', 'builtin.h', 'builtin-foreign.c',
]

nickle_files = [
  'builtin.5c', 'math.5c', 'scanf.5c', 'mutex.5c',
  'arc4.5c', 'prng.5c', 'command.5c', 'abort.5c',
  'printf.5c', 'history.5c', 'ctype.5c', 'string.5c', 'socket.5c',
  'file.5c', 'parse-args.5c', 'svg.5c', 'process.5c',
  'prime_sieve.5c', 'factorial.5c', 'gamma.5c', 'sort.5c', 'list.5c', 'skiplist.5c',
  'json.5c', 'cha-cha.5c',
]

nickle_headers = [
  'builtin.h', 'mem.h', 'nickle.h', 'ref.h', 'value.h',
  'builtin-namespaces.h', 'memp.h', 'opcode.h', 'stack.h'
]
  
nickle_start = files('builtin.5c')[0]

prog_bison = find_program('bison', 'byacc', 'yacc', required : true)

gram_ch = custom_target(
  'gram.[ch]',
  input : 'gram.y',
  output : ['gram.c', 'gram.h'],
  command : [  prog_bison, '-o', '@OUTPUT0@', '-d', '@INPUT@'],
  install: true,
  install_dir: [false, nickle_includedir])

prog_flex = find_program('flex', 'lex', required : true)

lex_c = custom_target(
  'lex.c',
  input : 'lex.l',
  output : 'lex.c',
  command : [prog_flex, '-o', '@OUTPUT@', '@INPUT@'])

nickle_srcs += [gram_ch, lex_c]

configure_file(output : 'nickle-config.h',
	       configuration : conf_data,
	       install_dir : nickle_includedir)

install_headers(nickle_headers,
		install_dir: nickle_includedir)

install_data(nickle_files,
	     install_dir: nickle_libdir)

nickle = executable('nickle', nickle_srcs,
		    dependencies : [ readline_dep, math_dep ],
		    include_directories : include_directories('.'),
		    link_args: link_args,
		    install: true)

manual = configure_file(output : 'nickle.1',
			input : 'nickle.1.in',
			configuration: conf_data)

install_man(manual)

test_env = environment(
  {
    'NICKLESTART': meson.current_source_dir() / 'builtin.5c',
    'NICKLEPATH': meson.current_source_dir()
  }
  )

subdir('test')
subdir('examples')
subdir('bench')
subdir('doc')
