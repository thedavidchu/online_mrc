quickmrc_include = include_directories('include',
    '../../lib/common_headers/include',
    '../../lib/histogram/include',
)

quickmrc_buckets_lib = library('quickmrc_buckets_lib',
    'buckets.c',
    include_directories : [
        common_headers_include,
        quickmrc_include,
    ],
    dependencies : glib_dep,
)

quickmrc_lib = library('quickmrc_lib',
    'quickmrc.c',
    include_directories : [
        common_headers_include,
        histogram_include,
        quickmrc_include,
        hash_include,
    ],
    dependencies : glib_dep,
    link_with : [
        basic_histogram_lib,
        splitmix64_lib,
        quickmrc_buckets_lib,
    ])
