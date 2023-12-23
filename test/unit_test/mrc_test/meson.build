olken_test_exe = executable('olken_test_exe',
    'olken_test.c',
    include_directories : [
        miss_rate_curve_include,
        mytester_include,
        olken_include,
        random_include,
        tree_include,
    ],
    dependencies : glib_dep,
    link_with : [
        basic_miss_rate_curve_lib,
        olken_lib,
        zipfian_random_lib,
    ])

fixed_size_shards_test_exe = executable('fixed_size_shards_test_exe',
    'fixed_size_shards_test.c',
    include_directories : [
        fixed_size_shards_include,
        miss_rate_curve_include,
        mytester_include,
        olken_include,
        random_include,
        tree_include,
    ],
    dependencies : glib_dep,
    link_with : [
        basic_miss_rate_curve_lib,
        fixed_size_shards_lib,
        olken_lib,
        zipfian_random_lib,
    ])

mimir_test_exe = executable('mimir_test_exe',
    'mimir_test.c',
    include_directories : [
        mimir_include,
        mytester_include,
        miss_rate_curve_include,
        olken_include,
        private_mimir_include,
        random_include,
        tree_include,
    ],
    dependencies : glib_dep,
    link_with : [
        basic_miss_rate_curve_lib,
        mimir_lib,
        olken_lib,
        zipfian_random_lib,
    ])

test('olken_test', olken_test_exe)
test('fixed_size_shards_test', fixed_size_shards_test_exe)
test('mimir_test', mimir_test_exe, timeout : 0)
