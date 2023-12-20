tree_test_exe = executable('tree_test_exe',
    'tree_test.c',
    include_directories : [common_headers_include, mytester_include, tree_include],
    link_with : [basic_tree_lib, sleator_tree_lib])

test('tree_test', tree_test_exe)
