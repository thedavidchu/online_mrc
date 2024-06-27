file_test_exe = executable(
    'file_test_exe',
    'file_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        file_dep,
        hash_dep,
    ],
)

test('file_test', file_test_exe)