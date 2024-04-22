mrc_performance_test_exe = executable(
    'mrc_performance_test_exe',
    'mrc_performance_test.c',
    include_directories: [
        mytester_include,
        tree_include,
        random_include,
        olken_include,
        fixed_size_shards_include,
        fixed_rate_shards_include,
        mimir_include,
    ],
    dependencies: [
        glib_dep,
        splitmix64_dep,
        basic_histogram_dep,
        fractional_histogram_dep,
    ],
    link_with: [
        olken_lib,
        fixed_size_shards_lib,
        parda_fixed_rate_shards_lib,
        mimir_lib,
        zipfian_random_lib,
    ],
)

test('mrc_performance_test', mrc_performance_test_exe, timeout: 0)
