mytester_include = include_directories('include')

# Tests
reuse_stack_test_exe = executable('reuse_stack_test_exe',
    'reuse_stack_test.c',
    include_directories : [
        mytester_include,
        tree_include,
        random_include,
        reuse_stack_include,
    ],
    dependencies : glib_dep,
    link_with : [
        olken_reuse_stack_lib,
        fixed_size_shards_lib,
        zipfian_random_lib,
    ])

subdir('unit_test')

# Run tests
test('reuse_stack_test', reuse_stack_test_exe)
