# Tests
tree_test_exe = executable('tree_test_exe',
    'tree_test.c',
    include_directories : tree_include,
    link_with : [naive_tree_lib, sleator_tree_lib])

reuse_stack_test_exe = executable('reuse_stack_test_exe',
    'reuse_stack_test.c',
    include_directories : reuse_stack_include,
    link_with : [reuse_stack_lib])

# Run tests
test('test_tree', tree_test_exe)
test('test_reuse_stack', reuse_stack_test_exe)
