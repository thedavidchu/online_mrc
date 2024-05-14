reservoir_sampling_mrc_dep = declare_dependency(
    link_with: library(
        'reservoir_sampling_mrc_lib',
        'reservoir_sampling_mrc.c',
        include_directories: include_directories('include'),
        dependencies: [
            sleator_tree_dep,
            common_dep,
            histogram_dep,
            glib_dep,
            hash_dep,
            lookup_dep,
            olken_dep,
            sampling_dep,
        ],
    ),
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
        histogram_dep,
        glib_dep,
        tree_dep,
        lookup_dep,
        olken_dep,
        sampling_dep,
    ],
)