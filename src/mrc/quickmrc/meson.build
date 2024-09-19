quickmrc_buckets_dep = declare_dependency(
    link_with: library(
        'quickmrc_buckets_lib',
        'quickmrc_buckets.c',
        'buckets.c',
        include_directories: include_directories('include'),
        dependencies: [glib_dep, common_dep, thread_dep],
        c_args: [
            # NOTE  I need the compiler to support AVX and AVX2. I guess
            #       the QuickMRC code doesn't support AVX512. I wonder
            #       if that would prove to be faster.
            '-mavx2',
        ],
    ),
)

quickmrc_dep = declare_dependency(
    link_with: library(
        'quickmrc_lib',
        'quickmrc.c',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
            histogram_dep,
            glib_dep,
            hash_dep,
            quickmrc_buckets_dep,
            lookup_dep,
            miss_rate_curve_dep,
            shards_dep,
        ],
    ),
    dependencies: [
        glib_dep,
        lookup_dep,
        shards_dep,
    ],
    include_directories: include_directories('include'),
)

evicting_quickmrc_dep = declare_dependency(
    link_with: library(
        'evicting_quickmrc_lib',
        [
            'evicting_quickmrc.c',
        ],
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
            quickmrc_buckets_dep,
        ],
        c_args: [
            # NOTE  I need the compiler to support AVX and AVX2. I guess
            #       the QuickMRC code doesn't support AVX512. I wonder
            #       if that would prove to be faster.
            '-mavx2',
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

bucketed_quickmrc_dep = declare_dependency(
    link_with: library(
        'bucketed_quickmrc_lib',
        'bucketed_quickmrc.c',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
            histogram_dep,
            glib_dep,
            hash_dep,
            quickmrc_buckets_dep,
            lookup_dep,
        ],
    ),
    dependencies: [
        glib_dep,
        lookup_dep,
    ],
    include_directories: include_directories('include'),
)