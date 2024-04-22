mimir_include = include_directories(
    'include',
)

private_mimir_include = include_directories(
    'private_include',
    'include',
)

mimir_buckets_lib = library(
    'mimir_buckets_lib',
    'buckets.c',
    include_directories: [
        mimir_include,
        private_mimir_include,
    ],
    dependencies: [common_dep, glib_dep],
)

mimir_lib = library(
    'mimir_lib',
    'mimir.c',
    include_directories: [
        mimir_include,
    ],
    dependencies: [common_dep, fractional_histogram_dep, glib_dep],
    link_with: [
        mimir_buckets_lib,
    ],
)
