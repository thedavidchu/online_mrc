tree_test_exe = executable('tree_test_exe',
    'tree_test.c',
    include_directories : [mytester_include, tree_include],
    link_with : [naive_tree_lib, sleator_tree_lib])

test('tree_test', tree_test_exe)
