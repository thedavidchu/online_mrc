mrc_performance_test_exe = executable(
    'mrc_performance_test_exe',
    'mrc_performance_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        histogram_dep,
        evicting_map_dep,
        common_dep,
        fixed_rate_shards_dep,
        fixed_size_shards_dep,
        fractional_histogram_dep,
        glib_dep,
        mimir_dep,
        olken_dep,
        parda_fixed_rate_shards_dep,
        quickmrc_dep,
        bucketed_quickmrc_dep,
        timer_dep,
        tree_dep,
        zipfian_random_dep,
    ],
)

test('mrc_performance_test', mrc_performance_test_exe, timeout: 0)