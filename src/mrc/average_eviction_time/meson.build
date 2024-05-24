average_eviction_time_inc = include_directories('include')

average_eviction_time_lib = library(
    'average_eviction_time_lib',
    'average_eviction_time.c',
    include_directories: average_eviction_time_inc,
    dependencies: [
        common_dep,
        io_dep,
        lookup_dep,
        histogram_dep,
        miss_rate_curve_dep,
    ],
)

average_eviction_time_dep = declare_dependency(
    link_with: average_eviction_time_lib,
    include_directories: average_eviction_time_inc,
    dependencies: [
        lookup_dep,
        histogram_dep,
        miss_rate_curve_dep,
    ],
)