io_inc = include_directories('include')

io_lib = library(
    'io_lib',
    'io.c',
    include_directories: io_inc,
    dependencies: [
        common_dep,
    ],
)

io_dep = declare_dependency(
    link_with: io_lib,
    include_directories: io_inc,
)