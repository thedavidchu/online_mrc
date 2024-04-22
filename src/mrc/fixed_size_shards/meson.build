fixed_size_shards_dep = declare_dependency(
    link_with: library(
        'fixed_size_shards_lib',
        'fixed_size_shards.c',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
            glib_dep,
            splitmix64_dep,
            basic_histogram_dep,
            splay_priority_queue_dep,
            basic_tree_dep,
            sleator_tree_dep,
        ],
    ),
    include_directories: include_directories('include'),
    dependencies: [
        splay_priority_queue_dep,
    ],
)
