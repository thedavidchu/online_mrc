splay_priority_queue_test_exe = executable(
    'splay_priority_queue_test_exe',
    'splay_priority_queue_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        hash_dep,
        priority_queue_dep,
    ],
)

heap_priority_queue_test_exe = executable(
    'heap_priority_queue_test_exe',
    'heap_priority_queue_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        hash_dep,
        priority_queue_dep,
    ],
)

test('splay_priority_queue_test', splay_priority_queue_test_exe)
test('heap_priority_queue_test', heap_priority_queue_test_exe)