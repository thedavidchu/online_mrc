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
    ],
)

parallel_lookup_test_exe = executable(
    'parallel_lookup_test_exe',
    'parallel_lookup_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        lookup_dep,
        thread_dep,
    ],
)

evicting_lookup_test_exe = executable(
    'evicting_lookup_test_exe',
    'evicting_lookup_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        lookup_dep,
        hash_dep,
    ],
)

test('lookup_test', lookup_test_exe)
test('parallel_lookup_test', parallel_lookup_test_exe)
test('evicting_lookup_test', evicting_lookup_test_exe)