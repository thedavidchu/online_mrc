evicting_quickmrc_dep = declare_dependency(
    link_with: library(
        'evicting_quickmrc_lib',
        'evicting_quickmrc.c',
        'qmrc.c',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
            histogram_dep,
            glib_dep,
            hash_dep,
            lookup_dep,
            miss_rate_curve_dep,
            # These are part of the interval statistics
            interval_statistics_dep,
        ],
    ),
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
        histogram_dep,
        glib_dep,
        lookup_dep,
        # These are part of the interval statistics
        interval_statistics_dep,
    ],
)