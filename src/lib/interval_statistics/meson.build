interval_statistics_inc = include_directories('include')

interval_statistics_lib = library(
    'interval_statistics_lib',
    'interval_statistics.c',
    include_directories: interval_statistics_inc,
    dependencies: [
        histogram_dep,
        file_dep,
    ],
)

interval_statistics_dep = declare_dependency(
    link_with: interval_statistics_lib,
    include_directories: interval_statistics_inc,
    dependencies: [
        histogram_dep,
    ],
)