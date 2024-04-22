priority_queue_test_exe = executable(
    'priority_queue_test_exe',
    'priority_queue_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        splitmix64_dep,
        splay_priority_queue_dep,
    ],
)

test('priority_queue_test', priority_queue_test_exe)
