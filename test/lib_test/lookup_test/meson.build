lookup_test_exe = executable(
    'lookup_test_exe',
    'lookup_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        lookup_dep,
        hash_dep,
        thread_dep,
    ],
)

test('lookup_test', lookup_test_exe)
