histogram_test_exe = executable('histogram_test_exe',
    'histogram_test.c',
    include_directories : [mytester_include, histogram_include],
    dependencies : glib_dep,
    link_with : [basic_histogram_lib])

test('histogram_test', histogram_test_exe)
