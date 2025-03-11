cpp_cache_inc = include_directories('include')

# Cache Access
cache_access_dep = declare_dependency(
    link_with: library(
        'cache_access_lib',
        'cache_access.cpp',
        include_directories: cpp_cache_inc,
        dependencies: [
            common_dep,
            trace_dep,
        ],
    ),
    include_directories: cpp_cache_inc,
    dependencies: [
        common_dep,
        trace_dep,
    ],
)

# Cache Metadata
cache_metadata_dep = declare_dependency(
    link_with: library(
        'cache_metadata_lib',
        'cache_metadata.cpp',
        include_directories: cpp_cache_inc,
        dependencies: [
            io_dep,
            cache_access_dep,
        ],
    ),
    include_directories: cpp_cache_inc,
    dependencies: [
        io_dep,
    ],
)

# Cache Statistics
cache_statistics_dep = declare_dependency(
    link_with: library(
        'cache_statistics_lib',
        'cache_statistics.cpp',
        include_directories: [cpp_cache_inc],
        dependencies: common_dep,
    ),
    include_directories: cpp_cache_inc,
)

# Cache Trace
cache_trace_dep = declare_dependency(
    link_with: library(
        'cache_trace_lib',
        'cache_trace.cpp',
        include_directories: cpp_cache_inc,
        dependencies: [
            common_dep,
            io_dep,
            trace_dep,
            cache_access_dep,
        ],
    ),
    dependencies: [
        common_dep,
        io_dep,
    ],
)

# Bundled dependencies
cpp_cache_dep = declare_dependency(
    dependencies: [
        cache_access_dep,
        cache_metadata_dep,
        cache_statistics_dep,
        cache_trace_dep,
    ],
    include_directories: cpp_cache_inc,
)