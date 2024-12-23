sieve_cache_test_exe = executable(
    'sieve_cache_test_exe',
    'sieve_cache_test.cpp',
    include_directories: [
        mytester_include,
        cache_inc,
    ],
    link_with: yang_sieve_cache_lib,
    dependencies: [
        common_dep,
        trace_dep,
        yang_cache_dep,
    ],
)

yang_sieve_cache_test_exe = executable(
    'yang_sieve_cache_test_exe',
    'yang_sieve_cache_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        lib_cache_sim_dep,
        yang_cache_dep,
    ],
)

ttl_clock_cache_test_exe = executable(
    'ttl_clock_cache_test_exe',
    'ttl_clock_cache_test.cpp',
    include_directories: [
        mytester_include,
        cache_inc,
    ],
    dependencies: [
        common_dep,
        trace_dep,
    ],
)

test('sieve_cache_test', sieve_cache_test_exe, args: [test_trace])
test('yang_sieve_cache_test', yang_sieve_cache_test_exe)
test('ttl_clock_cache_test', ttl_clock_cache_test_exe, args: [test_trace])