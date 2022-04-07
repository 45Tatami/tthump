project('tthump', 'c',
  version : '1.0.0',
  meson_version : '>=0.56.0',
  default_options : ['c_std=c11', 'warning_level=3', 'werror=true']
)

src = [
  'tthump.c',
  'util.c'
]

deps = []

libavutil_dep = dependency('libavutil', version : '>=57.17.100',)
deps += libavutil_dep
libswscale_dep = dependency('libswscale', version : '>=6.4.100')
deps += libswscale_dep
libavformat_dep = dependency('libavformat', version : '>=59.16.100')
deps += libavformat_dep
libavcodec_dep = dependency('libavcodec', version : '>=59.18.100')
deps += libavcodec_dep

deps += dependency('vpx')


# XSI extension required for at least realpath and implies POSIX 2001
add_project_arguments('-D_XOPEN_SOURCE=600', language : 'c')

# Sanity check for c11 threads
compiler = meson.get_compiler('c')
has_c11threads = compiler.has_function('thrd_create',
  prefix : '#include <threads.h>')

if not has_c11threads or not get_option('c11threads')
  add_project_arguments('-DNO_C11_THREADS', language : 'c')
  deps += dependency('threads')
endif

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_TTHUMP']

shlib = library('tthump', src,
  install : true,
  c_args : lib_args,
  gnu_symbol_visibility : 'hidden',
  dependencies : deps,
)

# Make this library usable as a Meson subproject.
tthump_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with : shlib)

# Make this library usable from the system's
# package manager.
install_headers('tthump.h', subdir : 'tthump')

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  name : 'tthump',
  filebase : 'tthump',
  description : 'Simple thumbnailer library',
  subdirs : 'tthump',
  libraries : shlib,
  version : '1.0.0',
)

subdir('tests')
