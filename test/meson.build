mytester_include = include_directories('include')

# Tests
histogram_test_exe = executable('histogram_test_exe',
    'histogram_test.c',
    include_directories : [mytester_include, histogram_include],
    dependencies : glib_dep,
    link_with : [basic_histogram_lib])

tree_test_exe = executable('tree_test_exe',
    'tree_test.c',
    include_directories : [mytester_include, tree_include],
    link_with : [naive_tree_lib, sleator_tree_lib])

reuse_stack_test_exe = executable('reuse_stack_test_exe',
    'reuse_stack_test.c',
    include_directories : [mytester_include, tree_include, reuse_stack_include],
    dependencies : glib_dep,
    link_with : [olken_reuse_stack_lib])

priority_queue_test_exe = executable('priority_queue_test_exe',
    'priority_queue_test.c',
    include_directories : [mytester_include, priority_queue_include],
    dependencies : glib_dep,
    link_with : [splay_priority_queue_lib])

subdir('unit_test')

# Run tests
test('histogram_test', histogram_test_exe)
test('tree_test', tree_test_exe)
test('reuse_stack_test', reuse_stack_test_exe)
test('priority_queue_test', priority_queue_test_exe)
