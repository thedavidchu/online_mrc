# Directory-wide include
priority_queue_include = include_directories(
    'include',
)

splay_priority_queue_lib = library(
    'splay_priority_queue_lib',
    'splay_priority_queue.c',
    include_directories: [
        priority_queue_include,
    ],
    dependencies: [common_dep, splitmix64_dep],
)
