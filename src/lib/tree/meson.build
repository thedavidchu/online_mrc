
naive_tree_inc = include_directories('include')
naive_tree_lib = library('naive_tree_lib',
    'naive_tree.c',
    include_directories : naive_tree_inc)
tree_test_exe = executable('tree_test_exe',
    'test_tree.c',
    include_directories : naive_tree_inc,
    link_with : naive_tree_lib)
test('test_naive_tree', tree_test_exe)