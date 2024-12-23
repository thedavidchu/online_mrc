clock_cache_test_exe = executable(
    'clock_cache_test_exe',
    'clock_cache_test.cpp',
    include_directories: [
        mytester_include,
        cache_inc,
    ],
    dependencies: [
        common_dep,
        trace_dep,
        yang_cache_dep,
    ],
)

sieve_cache_test_exe = executable(
    'sieve_cache_test_exe',
    'sieve_cache_test.cpp',
    include_directories: [
        mytester_include,
        cache_inc,
    ],
    dependencies: [
        common_dep,
        trace_dep,
        yang_cache_dep,
    ],
)

test('clock_cache_test', clock_cache_test_exe, args: [test_trace])
test('sieve_cache_test', sieve_cache_test_exe, args: [test_trace])