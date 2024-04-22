olken_dep = declare_dependency(
    link_with: library(
        'olken_lib',
        'olken.c',
        include_directories: include_directories('include'),
        dependencies: [
            sleator_tree_dep,
            common_dep,
            basic_histogram_dep,
            glib_dep,
            splitmix64_dep,
        ],
    ),
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
        basic_histogram_dep,
        glib_dep,
        tree_dep,
    ],
)
