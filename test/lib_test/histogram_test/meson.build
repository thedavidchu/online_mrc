histogram_test_exe = executable(
    'histogram_test_exe',
    'histogram_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [common_dep, basic_histogram_dep, glib_dep],
)

test('histogram_test', histogram_test_exe)
