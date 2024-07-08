io_test_exe = executable(
    'io_test_exe',
    'io_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        io_dep,
        hash_dep,
    ],
)

test('io_test', io_test_exe, args: [test_trace])