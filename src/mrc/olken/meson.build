olken_include = include_directories(
    'include',
    '../../lib/common_headers/include',
    '../../lib/tree/include',
)

olken_lib = library(
    'olken_lib',
    'olken.c',
    include_directories: [
        common_headers_include,
        tree_include,
        olken_include,
    ],
    dependencies: [basic_histogram_dep, glib_dep, splitmix64_dep],
    link_with: [
        basic_tree_lib,
        sleator_tree_lib,
    ],
)
