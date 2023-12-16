olken_include = include_directories('include',
    '../../lib/common_headers/include',
    '../../lib/hash/include',
    '../../lib/histogram/include',
    )

olken_lib = library('olken_lib',
    'olken.c',
    include_directories : [
        common_headers_include,
        histogram_include,
        tree_include,
        olken_include,
    ],
    dependencies : glib_dep,
    link_with : [
        basic_histogram_lib,
        basic_tree_lib,
        sleator_tree_lib,
    ])
