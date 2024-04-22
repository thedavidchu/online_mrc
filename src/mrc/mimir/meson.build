mimir_buckets_dep = declare_dependency(
    link_with: library(
        'mimir_buckets_lib',
        'buckets.c',
        include_directories: include_directories('include', 'private_include'),
        dependencies: [common_dep, glib_dep],
    ),
    include_directories: include_directories('include', 'private_include'),
)

mimir_dep = declare_dependency(
    link_with: library(
        'mimir_lib',
        'mimir.c',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
            fractional_histogram_dep,
            glib_dep,
            mimir_buckets_dep,
        ],
    ),
    include_directories: include_directories('include'),
)
