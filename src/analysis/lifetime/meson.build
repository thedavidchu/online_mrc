lifetime_inc = include_directories('include')

lifetime_list_lib = library(
    'lifetime_list_lib',
    'lifetime_list.cpp',
    include_directories: [cache_inc, lifetime_inc],
    dependencies: [
        common_dep,
        # These are for the cache_inc
        io_dep,
        trace_dep,
    ],
)

lifetime_exe = executable(
    'lifetime_exe',
    'lifetime.cpp',
    link_with: lifetime_list_lib,
    include_directories: [cache_inc, lifetime_inc],
    dependencies: [
        common_dep,
        glib_dep,
        file_dep,
        list_dep,
        io_dep,
        shards_dep,
        trace_dep,
    ],
)