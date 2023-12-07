naive_tree_inc = include_directories('include')
naive_tree_lib = library('naive_tree_lib',
    'naive_tree.c',
    include_directories : naive_tree_inc)
manual_tree_test_exe = executable('manual_tree_test_exe',
    'manual_test_tree.c',
    include_directories : naive_tree_inc,
    link_with : naive_tree_lib)
test('manual_test_naive_tree', manual_tree_test_exe)
