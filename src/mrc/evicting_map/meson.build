evicting_map_dep = declare_dependency(
    link_with: library(
        'evicting_map_lib',
        'evicting_map.c',
        include_directories: include_directories('include'),
        dependencies: [
            sleator_tree_dep,
            common_dep,
            histogram_dep,
            glib_dep,
            hash_dep,
            lookup_dep,
            miss_rate_curve_dep,
            # These are part of the interval statistics
            interval_statistics_dep,
            # These are part of the statistics
            statistics_dep,
        ],
    ),
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
        histogram_dep,
        glib_dep,
        tree_dep,
        lookup_dep,
        # These are part of the interval statistics
        interval_statistics_dep,
        # These are part of the statistics
        statistics_dep,
    ],
)