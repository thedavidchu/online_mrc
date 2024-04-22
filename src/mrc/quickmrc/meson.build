quickmrc_include = include_directories(
    'include',
)

QUICK_TEST = false
if QUICK_TEST
    hash_table_test = executable(
        'hash_table_test',
        'hash_table.c',
        dependencies: [common_dep, splitmix64_dep],
    )
    test('hash_table_test', hash_table_test)
endif

hash_table_dep = declare_dependency(
    link_with: library(
        'hash_table_lib',
        'hash_table.c',
        dependencies: [splitmix64_dep, common_dep],
    ),
)

quickmrc_buckets_dep = declare_dependency(
    link_with: library(
        'quickmrc_buckets_lib',
        'buckets.c',
        include_directories: include_directories('include'),
        dependencies: [glib_dep, common_dep],
    ),
)

quickmrc_dep = declare_dependency(
    link_with: library(
        'quickmrc_lib',
        'quickmrc.c',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
            basic_histogram_dep,
            glib_dep,
            splitmix64_dep,
            quickmrc_buckets_dep,
            hash_table_dep,
        ],
    ),
    include_directories: include_directories('include'),
)
