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
        splitmix64_dep,
        basic_miss_rate_curve_dep,
        glib_dep,
        fixed_size_shards_dep,
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
        basic_miss_rate_curve_dep,
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
        basic_histogram_dep,
        basic_miss_rate_curve_dep,
        zipfian_random_dep,
        quickmrc_dep,
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
        splitmix64_dep,
        basic_histogram_dep,
        basic_miss_rate_curve_dep,
        zipfian_random_dep,
        olken_dep,
    ],
)

test('olken_test', olken_test_exe)
test('fixed_size_shards_test', fixed_size_shards_test_exe)
test('mimir_test', mimir_test_exe, timeout: 0)
test('parda_fixed_rate_shards_test', parda_fixed_rate_shards_test_exe)
test('quickmrc_test', quickmrc_test_exe)
