
# Tests
tree_test_exe = executable('tree_test_exe',
    'tree_test.c',
    include_directories : tree_include,
    link_with : [naive_tree_lib, sleator_tree_lib])
test('test_naive_tree', tree_test_exe)