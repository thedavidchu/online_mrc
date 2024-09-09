array_inc = include_directories('include')

binary64_array_lib = library(
    'binary64_array_lib',
    'binary64_array.c',
    include_directories: array_inc,
    dependencies: [
        common_dep,
        glib_dep,
        file_dep,
    ],
)

print_array_lib = library(
    'print_array_lib',
    'print_array.c',
    include_directories: array_inc,
    dependencies: [
        common_dep,
    ],
)

array_dep = declare_dependency(
    link_with: [
        binary64_array_lib,
        print_array_lib,
    ],
    include_directories: array_inc,
    dependencies: [
        glib_dep,
    ],
)