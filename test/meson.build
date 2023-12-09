mytester_include = include_directories('include')

# Tests
tree_test_exe = executable('tree_test_exe',
    'tree_test.c',
    include_directories : [mytester_include, tree_include],
    link_with : [naive_tree_lib, sleator_tree_lib])

reuse_stack_test_exe = executable('reuse_stack_test_exe',
    'reuse_stack_test.c',
    include_directories : [mytester_include, tree_include, reuse_stack_include],
    dependencies : glib_dep,
    link_with : [reuse_stack_lib])

# Run tests
test('test_tree', tree_test_exe)
test('test_reuse_stack', reuse_stack_test_exe)
