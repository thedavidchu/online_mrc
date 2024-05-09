fixed_size_shards_dep = declare_dependency(
    link_with: library(
        'fixed_size_shards_lib',
        'fixed_size_shards.c',
        include_directories: include_directories('include'),
        dependencies: [
            basic_tree_dep,
            common_dep,
            glib_dep,
            histogram_dep,
            my_hash_dep,
            sleator_tree_dep,
            splay_priority_queue_dep,
        ],
    ),
    include_directories: include_directories('include'),
    dependencies: [
        splay_priority_queue_dep,
        hash_dep,
    ],
)

fixed_rate_shards_dep = declare_dependency(
    link_with: library(
        'fixed_rate_shards_lib',
        'fixed_rate_shards.c',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
            glib_dep,
            olken_dep,
            hash_dep,
            histogram_dep,
            splay_priority_queue_dep,
            basic_tree_dep,
            sleator_tree_dep,
        ],
    ),
    include_directories: include_directories('include'),
    dependencies: [
        olken_dep,
        splay_priority_queue_dep,
    ],
)
