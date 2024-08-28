lookup_performance_test_exe = executable(
    'lookup_performance_test_exe',
    'lookup_performance_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        lookup_dep,
        timer_dep,
    ],
)

test('lookup_performance_test', lookup_performance_test_exe)