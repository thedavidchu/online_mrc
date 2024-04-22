priority_queue_test_exe = executable(
    'priority_queue_test_exe',
    'priority_queue_test.c',
    include_directories: [mytester_include, priority_queue_include],
    dependencies: [glib_dep, splitmix64_dep],
    link_with: [splay_priority_queue_lib],
)

test('priority_queue_test', priority_queue_test_exe)
