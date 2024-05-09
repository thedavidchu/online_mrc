hash_test_exe = executable(
    'hash_test_exe',
    'hash_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        hash_dep,
        hash_dep,
    ],
)

test('hash_test', hash_test_exe)
