olken_include = include_directories(
    'include',
    '../../lib/tree/include',
)

olken_lib = library(
    'olken_lib',
    'olken.c',
    include_directories: [
        olken_include,
    ],
    dependencies: [
        basic_tree_dep,
        sleator_tree_dep,
        common_dep,
        basic_histogram_dep,
        glib_dep,
        splitmix64_dep,
    ],
)
