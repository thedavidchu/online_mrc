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

unhash_test_exe = executable(
    'unhash_test_exe',
    'unhash_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        glib_dep,
        hash_dep,
    ],
)

test('hash_test', hash_test_exe)
test('unhash_test', unhash_test_exe)