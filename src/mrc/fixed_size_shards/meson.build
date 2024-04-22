fixed_size_shards_include = include_directories(
    'include',
    '../../lib/priority_queue/include',
)

fixed_size_shards_lib = library(
    'fixed_size_shards_lib',
    'fixed_size_shards.c',
    include_directories: [
        fixed_size_shards_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        splitmix64_dep,
        basic_histogram_dep,
        splay_priority_queue_dep,
        basic_tree_dep,
        sleator_tree_dep,
    ],
)
