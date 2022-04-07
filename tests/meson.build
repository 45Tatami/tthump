# Utils functions are hidden in shlib so build it again for testing
util_testlib = static_library('tthump_util_test', '../util.c',
  install : false,
  c_args : lib_args,
  dependencies : deps,
)

incdir = include_directories('../')

# These are used for the tests but they will find them themselves and skip if
# not found
touch = find_program('touch', required : false)
md5sum = find_program('md5sum', required : false)

test_error_exe = executable('tthump_error_test', 'tth_error_test.c',
  include_directories : incdir,
  link_with : shlib)
test_jobs_exe = executable('tthump_jobs_test', 'tth_jobs_test.c',
  include_directories : incdir,
  link_with : shlib)

test_util_mtime_exe = executable('tthumb_util_mtime_test',
  'tth_util_mtime.c', link_with : util_testlib,
  include_directories : incdir,
)
test_util_md5_exe = executable('tthumb_util_md5_test',
  'tth_util_md5.c', link_with : util_testlib,
  include_directories : incdir,
)
test_util_list_exe = executable('tthumb_util_list_test',
  'tth_util_list.c', link_with : util_testlib,
  include_directories : incdir,
)

test('Errors', test_error_exe)
test('Async Jobs', test_jobs_exe)

test('Util - Mtime', test_util_mtime_exe, args : touch.full_path())
test('Util - MD5', test_util_md5_exe, args : md5sum.full_path())
test('Util - List', test_util_list_exe)

