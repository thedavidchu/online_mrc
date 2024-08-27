hash_performance_test_exe = executable(
    'hash_performance_test_exe',
    'hash_performance_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        hash_dep,
        timer_dep,
    ],
)

test('hash_performance_test', hash_performance_test_exe)