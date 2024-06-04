priority_queue_inc = include_directories('include')

splay_priority_queue_lib = library(
    'splay_priority_queue_lib',
    'splay_priority_queue.c',
    include_directories: priority_queue_inc,
    dependencies: [
        common_dep,
        hash_dep,
    ],
)

heap_lib = library(
    'heap_lib',
    'heap.c',
    include_directories: priority_queue_inc,
    dependencies: [
        common_dep,
        hash_dep,
    ],
)

priority_queue_dep = declare_dependency(
    link_with: [
        splay_priority_queue_lib,
        heap_lib,
    ],
    include_directories: priority_queue_inc,
    dependencies: [
        common_dep,
        hash_dep,
    ],
)