sieve_cache_test_exe = executable(
    'sieve_cache_test_exe',
    'sieve_cache_test.cpp',
    include_directories: [
        mytester_include,
        cache_inc,
    ],
    dependencies: [
        common_dep,
    ],
)

test('sieve_cache_test', sieve_cache_test_exe)