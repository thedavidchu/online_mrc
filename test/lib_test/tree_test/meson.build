tree_test_exe = executable(
    'tree_test_exe',
    'tree_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        basic_tree_dep,
        sleator_tree_dep,
        common_dep,
    ],
)

test('tree_test', tree_test_exe)
