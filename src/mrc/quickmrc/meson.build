quickmrc_buckets_dep = declare_dependency(
    link_with: library(
        'quickmrc_buckets_lib',
        'buckets.c',
        include_directories: include_directories('include'),
        dependencies: [glib_dep, common_dep, dependency('threads')],
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
            splitmix64_dep,
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

bucketed_quickmrc_dep = declare_dependency(
    link_with: library(
        'bucketed_quickmrc_lib',
        'bucketed_quickmrc.c',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
            histogram_dep,
            glib_dep,
            splitmix64_dep,
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
