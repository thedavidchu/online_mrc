miss_rate_curve_dep = declare_dependency(
    link_with: library(
        'miss_rate_curve_lib',
        'miss_rate_curve.c',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
            glib_dep,
            basic_histogram_dep,
            fractional_histogram_dep,
        ],
    ),
    include_directories: include_directories('include'),
    dependencies: common_dep,
)
