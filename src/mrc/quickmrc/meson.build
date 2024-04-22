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
else
    hash_table_lib = library(
        'hash_table_lib',
        'hash_table.c',
        dependencies: [splitmix64_dep, common_dep],
    )
endif


quickmrc_buckets_lib = library(
    'quickmrc_buckets_lib',
    'buckets.c',
    include_directories: [
        quickmrc_include,
    ],
    dependencies: [glib_dep, common_dep],
)

quickmrc_lib = library(
    'quickmrc_lib',
    'quickmrc.c',
    include_directories: [
        quickmrc_include,
    ],
    link_with: [
        quickmrc_buckets_lib,
        hash_table_lib,
    ],
    dependencies: [common_dep, basic_histogram_dep, glib_dep, splitmix64_dep],
)
