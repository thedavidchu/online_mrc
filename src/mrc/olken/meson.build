olken_dep = declare_dependency(
    link_with: library(
        'olken_lib',
        'olken.c',
        include_directories: include_directories('include'),
        dependencies: [
            sleator_tree_dep,
            common_dep,
            histogram_dep,
            glib_dep,
            hash_dep,
            lookup_dep,
            miss_rate_curve_dep,
        ],
    ),
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
        histogram_dep,
        glib_dep,
        tree_dep,
        lookup_dep,
        miss_rate_curve_dep,
    ],
)

olken_with_ttl_dep = declare_dependency(
    link_with: library(
        'olken_with_ttl_lib',
        'olken_with_ttl.c',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
            glib_dep,
            hash_dep,
            histogram_dep,
            lookup_dep,
            miss_rate_curve_dep,
            olken_dep,
            priority_queue_dep,
            sleator_tree_dep,
        ],
    ),
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
        glib_dep,
        histogram_dep,
        lookup_dep,
        miss_rate_curve_dep,
        olken_dep,
        priority_queue_dep,
        tree_dep,
    ],
)