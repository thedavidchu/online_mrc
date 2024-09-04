statistics_inc = include_directories('include')

statistics_lib = library(
    'statistics_lib',
    'statistics.c',
    include_directories: statistics_inc,
    dependencies: [
        array_dep,
        common_dep,
    ],
)

statistics_dep = declare_dependency(
    link_with: statistics_lib,
    include_directories: statistics_inc,
    dependencies: [
        array_dep,
    ],
)