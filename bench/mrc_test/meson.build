mrc_performance_test_exe = executable(
    'mrc_performance_test_exe',
    'mrc_performance_test.c',
    include_directories: [
        mytester_include,
        fixed_rate_shards_include,
    ],
    dependencies: [
        common_dep,
        tree_dep,
        olken_dep,
        glib_dep,
        splitmix64_dep,
        basic_histogram_dep,
        fractional_histogram_dep,
        fixed_size_shards_dep,
        zipfian_random_dep,
        mimir_dep,
    ],
    link_with: [
        parda_fixed_rate_shards_lib,
    ],
)

test('mrc_performance_test', mrc_performance_test_exe, timeout: 0)
