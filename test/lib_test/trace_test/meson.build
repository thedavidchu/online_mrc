trace_test_exe = executable(
    'trace_test_exe',
    'trace_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        glib_dep,
        trace_dep,
    ],
)

test('trace_test', trace_test_exe)
