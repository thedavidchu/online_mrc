arrays_inc = include_directories('include')

arrays_lib = library(
    'arrays_lib',
    'arrays.c',
    include_directories: arrays_inc,
    dependencies: [
        common_dep,
        glib_dep,
        file_dep,
    ],
)

arrays_dep = declare_dependency(
    link_with: arrays_lib,
    include_directories: arrays_inc,
    dependencies: [
        glib_dep,
    ],
)