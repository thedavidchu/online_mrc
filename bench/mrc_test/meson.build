mrc_performance_test_exe = executable(
    'mrc_performance_test_exe',
    'mrc_performance_test.c',
    include_directories: [
        mytester_include,
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
        parda_fixed_rate_shards_dep,
        zipfian_random_dep,
        mimir_dep,
        quickmrc_dep,
    ],
)

test('mrc_performance_test', mrc_performance_test_exe, timeout: 0)

mrc_buckets_test_exe = executable(
    'mrc_buckets_test_exe',
    'mrc_buckets_test.c',
    include_directories: [
        mytester_include,
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
        parda_fixed_rate_shards_dep,
        zipfian_random_dep,
        mimir_dep,
        quickmrc_dep,
    ],
)

test('mrc_buckets_test', mrc_buckets_test_exe, timeout: 0)
