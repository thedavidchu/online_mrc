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

debug_tree_test_exe = executable(
    'debug_tree_test_exe',
    'debug_tree_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        basic_tree_dep,
        sleator_tree_dep,
        common_dep,
    ],
)

test('debug_tree_test', debug_tree_test_exe)
test('tree_test', tree_test_exe)