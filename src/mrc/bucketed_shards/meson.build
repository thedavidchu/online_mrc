bucketed_shards_dep = declare_dependency(
    link_with: library(
        'bucketed_shards_lib',
        'bucketed_shards.c',
        include_directories: include_directories('include'),
        dependencies: [
            sleator_tree_dep,
            common_dep,
            basic_histogram_dep,
            glib_dep,
            splitmix64_dep,
            lookup_dep,
        ],
    ),
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
        basic_histogram_dep,
        glib_dep,
        tree_dep,
        lookup_dep,
    ],
)
