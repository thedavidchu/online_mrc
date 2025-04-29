cpp_lib_inc = include_directories('include')

cpp_lib_util_dep = declare_dependency(
    link_with: library(
        'cpp_lib_util_lib',
        'util.cpp',
        include_directories: cpp_lib_inc,
        dependencies: [
            common_dep,
            boost_dep,
        ],
    ),
    include_directories: cpp_lib_inc,
)

cache_command_dep = declare_dependency(
    link_with: library(
        'cache_command_lib',
        'cache_command.cpp',
        include_directories: cpp_lib_inc,
    ),
    include_directories: cpp_lib_inc,
)

cache_trace_format = declare_dependency(
    link_with: library(
        'cache_trace_format_lib',
        'cache_trace_format.cpp',
        include_directories: cpp_lib_inc,
    ),
    include_directories: cpp_lib_inc,
)

cache_access_dep = declare_dependency(
    link_with: library(
        'cache_access_lib',
        'cache_access.cpp',
        include_directories: cpp_lib_inc,
        dependencies: [
            common_dep,
            trace_dep,
            cache_trace_format,
        ],
    ),
    include_directories: cpp_lib_inc,
    dependencies: [
        common_dep,
        trace_dep,
        cache_trace_format,
    ],
)

cache_metadata_dep = declare_dependency(
    link_with: library(
        'cache_metadata_lib',
        'cache_metadata.cpp',
        include_directories: cpp_lib_inc,
        dependencies: [
            io_dep,
            cache_access_dep,
        ],
    ),
    include_directories: cpp_lib_inc,
    dependencies: [
        io_dep,
    ],
)

cache_statistics_dep = declare_dependency(
    link_with: library(
        'cache_statistics_lib',
        'cache_statistics.cpp',
        include_directories: [cpp_lib_inc],
        dependencies: common_dep,
    ),
    include_directories: cpp_lib_inc,
)

cache_trace_dep = declare_dependency(
    link_with: library(
        'cache_trace_lib',
        'cache_trace.cpp',
        include_directories: cpp_lib_inc,
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
cpp_lib_dep = declare_dependency(
    dependencies: [
        cpp_lib_util_dep,
        cache_access_dep,
        cache_metadata_dep,
        cache_statistics_dep,
        cache_trace_dep,
        cache_trace_format,
        cache_command_dep,
    ],
    include_directories: cpp_lib_inc,
)