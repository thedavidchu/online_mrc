trace_inc = include_directories('include')

trace_lib = library(
    'trace_lib',
    [
        'generator.c',
        'reader.c',
        'trace.c',
    ],
    include_directories: trace_inc,
    dependencies: [
        common_dep,
        glib_dep,
        io_dep,
        zipfian_random_dep,
    ],
)

trace_dep = declare_dependency(
    link_with: trace_lib,
    include_directories: trace_inc,
    dependencies: [],
)