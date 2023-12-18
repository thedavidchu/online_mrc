mimir_include = include_directories('include',
    '../../lib/common_headers/include',
    '../../lib/histogram/include',
)

mimir_lib = library('mimir_lib',
    'mimir.c',
    include_directories : [
        common_headers_include,
        histogram_include,
        mimir_include,
    ],
    dependencies : glib_dep,
    link_with : [
        basic_tree_lib,
        fractional_histogram_lib,
        sleator_tree_lib,
    ])