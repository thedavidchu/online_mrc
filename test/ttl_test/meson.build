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
    ],
)

c_sieve_cache_test_exe = executable(
    'c_sieve_cache_test_exe',
    'c_sieve_cache_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        lib_cache_sim_dep,
    ],
)

test(
    'sieve_cache_test',
    sieve_cache_test_exe,
    args: [test_trace],
)