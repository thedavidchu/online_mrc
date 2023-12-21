olken_test_exe = executable('olken_test_exe',
    'olken_test.c',
    include_directories : [
        mytester_include,
        miss_rate_curve_include,
        tree_include,
        random_include,
        olken_include,
    ],
    dependencies : glib_dep,
    link_with : [
        olken_lib,
        zipfian_random_lib,
        basic_miss_rate_curve_lib,
    ])

fixed_size_shards_test_exe = executable('fixed_size_shards_test_exe',
    'fixed_size_shards_test.c',
    include_directories : [
        mytester_include,
        miss_rate_curve_include,
        tree_include,
        random_include,
        fixed_size_shards_include,
    ],
    dependencies : glib_dep,
    link_with : [
        fixed_size_shards_lib,
        zipfian_random_lib,
        basic_miss_rate_curve_lib,
    ])

mimir_test_exe = executable('mimir_test_exe',
    'mimir_test.c',
    include_directories : [
        mytester_include,
        miss_rate_curve_include,
        tree_include,
        random_include,
        mimir_include,
        private_mimir_include,
    ],
    dependencies : glib_dep,
    link_with : [
        mimir_lib,
        zipfian_random_lib,
        basic_miss_rate_curve_lib,
    ])

test('olken_test', olken_test_exe)
test('fixed_size_shards_test', fixed_size_shards_test_exe)
test('mimir_test', mimir_test_exe)
