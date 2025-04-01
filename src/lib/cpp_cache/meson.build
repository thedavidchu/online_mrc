cpp_cache_inc = include_directories('include')

cache_command_dep = declare_dependency(
    link_with: library(
        'cache_command_lib',
        'cache_command.cpp',
        include_directories: cpp_cache_inc,
    ),
    include_directories: cpp_cache_inc,
)

cache_trace_format = declare_dependency(
    link_with: library(
        'cache_trace_format_lib',
        'cache_trace_format.cpp',
        include_directories: cpp_cache_inc,
    ),
    include_directories: cpp_cache_inc,
)

# Cache Access
cache_access_dep = declare_dependency(
    link_with: library(
        'cache_access_lib',
        'cache_access.cpp',
        include_directories: cpp_cache_inc,
        dependencies: [
            common_dep,
            trace_dep,
            cache_trace_format,
        ],
    ),
    include_directories: cpp_cache_inc,
    dependencies: [
        common_dep,
        trace_dep,
        cache_trace_format,
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
            cache_trace_format,
        ],
    ),
    dependencies: [
        common_dep,
        io_dep,
        cache_trace_format,
    ],
)

# Bundled dependencies
cpp_cache_dep = declare_dependency(
    dependencies: [
        cache_access_dep,
        cache_metadata_dep,
        cache_statistics_dep,
        cache_trace_dep,
        cache_trace_format,
        cache_command_dep,
    ],
    include_directories: cpp_cache_inc,
)