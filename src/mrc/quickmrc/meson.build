quickmrc_include = include_directories(
    'include',
    '../../lib/common_headers/include',
    '../../lib/histogram/include',
)

QUICK_TEST = false
if QUICK_TEST
    hash_table_test = executable(
        'hash_table_test',
        'hash_table.c',
        include_directories: [
            hash_include,
            tree_include,
            common_headers_include,
        ],
    )
    test('hash_table_test', hash_table_test)
else
    hash_table_lib = library(
        'hash_table_lib',
        'hash_table.c',
        include_directories: [
            hash_include,
            tree_include,
            common_headers_include,
        ],
    )
endif


quickmrc_buckets_lib = library(
    'quickmrc_buckets_lib',
    'buckets.c',
    include_directories: [
        common_headers_include,
        quickmrc_include,
    ],
    dependencies: glib_dep,
)

quickmrc_lib = library(
    'quickmrc_lib',
    'quickmrc.c',
    include_directories: [
        common_headers_include,
        histogram_include,
        quickmrc_include,
        hash_include,
    ],
    dependencies: glib_dep,
    link_with: [
        basic_histogram_lib,
        splitmix64_lib,
        quickmrc_buckets_lib,
        hash_table_lib,
    ],
)
