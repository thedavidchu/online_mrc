olken_test_exe = executable(
    'olken_test_exe',
    'olken_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        glib_dep,
        olken_dep,
        zipfian_random_dep,
    ],
)

fixed_size_shards_test_exe = executable(
    'fixed_size_shards_test_exe',
    'fixed_size_shards_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        hash_dep,
        miss_rate_curve_dep,
        glib_dep,
        lookup_dep,
        shards_dep,
        olken_dep,
        zipfian_random_dep,
    ],
)

mimir_test_exe = executable(
    'mimir_test_exe',
    'mimir_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        glib_dep,
        miss_rate_curve_dep,
        mimir_dep,
        mimir_buckets_dep,
        olken_dep,
        zipfian_random_dep,
    ],
)

quickmrc_test_exe = executable(
    'quickmrc_test_exe',
    'quickmrc_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        glib_dep,
        histogram_dep,
        miss_rate_curve_dep,
        olken_dep,
        quickmrc_dep,
        zipfian_random_dep,
        lookup_dep,
    ],
)

parda_fixed_rate_shards_test_exe = executable(
    'parda_fixed_rate_shards_test_exe',
    'parda_fixed_rate_shards_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        glib_dep,
        parda_fixed_rate_shards_dep,
        hash_dep,
        histogram_dep,
        miss_rate_curve_dep,
        zipfian_random_dep,
        olken_dep,
    ],
)

fixed_rate_shards_test_exe = executable(
    'fixed_rate_shards_test_exe',
    'fixed_rate_shards_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        glib_dep,
        parda_fixed_rate_shards_dep,
        shards_dep,
        hash_dep,
        histogram_dep,
        miss_rate_curve_dep,
        zipfian_random_dep,
        olken_dep,
    ],
)

evicting_map_test_exe = executable(
    'evicting_map_test_exe',
    'evicting_map_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        glib_dep,
        evicting_map_dep,
        shards_dep,
        hash_dep,
        histogram_dep,
        miss_rate_curve_dep,
        zipfian_random_dep,
        olken_dep,
    ],
)

bucketed_quickmrc_test_exe = executable(
    'bucketed_quickmrc_test_exe',
    'bucketed_quickmrc_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        bucketed_quickmrc_dep,
        glib_dep,
        hash_dep,
        histogram_dep,
        miss_rate_curve_dep,
        zipfian_random_dep,
        olken_dep,
    ],
)

average_eviction_time_test_exe = executable(
    'average_eviction_time_test_exe',
    'average_eviction_time_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        average_eviction_time_dep,
        glib_dep,
        hash_dep,
        histogram_dep,
        miss_rate_curve_dep,
        zipfian_random_dep,
        olken_dep,
        trace_dep,
    ],
)

test('olken_test', olken_test_exe)
test('fixed_size_shards_test', fixed_size_shards_test_exe)

test('mimir_unit_test', mimir_test_exe, args: ['unit'])
test('mimir_rounder_test', mimir_test_exe, args: ['rounder'])
test('mimir_stacker_test', mimir_test_exe, args: ['stacker'])
if false
    # This is slow because it's not parallelized.
    test('mimir_test', mimir_test_exe, args: ['all'])
endif
test('parda_fixed_rate_shards_test', parda_fixed_rate_shards_test_exe)
test('fixed_rate_shards_test', fixed_rate_shards_test_exe)
test('quickmrc_test', quickmrc_test_exe)
test('evicting_map_test', evicting_map_test_exe)
test('bucketed_quickmrc_test', bucketed_quickmrc_test_exe)
test('average_eviction_time_test', average_eviction_time_test_exe)