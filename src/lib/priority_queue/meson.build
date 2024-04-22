splay_priority_queue_dep = declare_dependency(
    link_with: library(
        'splay_priority_queue_lib',
        'splay_priority_queue.c',
        include_directories: include_directories('include'),
        dependencies: [common_dep, splitmix64_dep],
    ),
    include_directories: include_directories('include'),
)
