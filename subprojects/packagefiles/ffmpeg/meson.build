project('FFmpeg', 'c', license : 'LGPL2.1+', version : '5.0.1')

cc = meson.get_compiler('c')

make = find_program('make')
configure = find_program('configure')

build_script = find_program('meson_build.sh')

run_command(build_script, check : true)

inc_dir = include_directories('ffmpeg_build/include')
lib_dir = meson.current_source_dir() / 'ffmpeg_build/lib'

libavutil     = cc.find_library('libavutil',     dirs : lib_dir, static : true)
libavcodec    = cc.find_library('libavcodec',    dirs : lib_dir, static : true)
libswscale    = cc.find_library('libswscale',    dirs : lib_dir, static : true)
libavformat   = cc.find_library('libavformat',   dirs : lib_dir, static : true)
libswresample = cc.find_library('libswresample', dirs : lib_dir, static : true)

shared_deps = [
  cc.find_library('m'),
  dependency('vpx'),
  dependency('zlib'),
]

# From ffmpeg wiki to allow linking to dynamic library
ld_args = [ '-Wl,-Bsymbolic' ]

libavcodec_dep = declare_dependency(
  include_directories : inc_dir,
  dependencies : [ libavcodec, libswresample ],
  link_args : ld_args,
  version : '59.18.100',
)
libavformat_dep = declare_dependency(
  include_directories : inc_dir,
  dependencies : [ libavformat ],
  link_args : ld_args,
  version : '59.16.100',
)
libswscale_dep = declare_dependency(
  include_directories : inc_dir,
  dependencies : [ libswscale ],
  link_args : ld_args,
  version : '6.4.100',
)
libavutil_dep = declare_dependency(
  include_directories : inc_dir,
  dependencies : [ libavutil, shared_deps ],
  link_args : ld_args,
  version : '57.17.100',
)

meson.override_dependency('libavcodec', libavcodec_dep)
meson.override_dependency('libavformat', libavformat_dep)
meson.override_dependency('libswscale', libswscale_dep)
meson.override_dependency('libavutil', libavutil_dep)
