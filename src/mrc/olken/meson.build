olken_include = include_directories(
    'include',
    '../../lib/tree/include',
)

olken_lib = library(
    'olken_lib',
    'olken.c',
    include_directories: [
        tree_include,
        olken_include,
    ],
    dependencies: [common_dep, basic_histogram_dep, glib_dep, splitmix64_dep],
    link_with: [
        basic_tree_lib,
        sleator_tree_lib,
    ],
)
