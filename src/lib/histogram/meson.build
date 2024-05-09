histogram_dep = declare_dependency(
    link_with: library(
        'histogram_lib',
        'histogram.c',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
        ],
    ),
    dependencies: [
        common_dep,
    ],
    include_directories: include_directories('include'),
)

fractional_histogram_dep = declare_dependency(
    link_with: library(
        'fractional_histogram_lib',
        'fractional_histogram.c',
        include_directories: [
            include_directories('include'),
        ],
        dependencies: common_dep,
    ),
    include_directories: include_directories('include'),
)
