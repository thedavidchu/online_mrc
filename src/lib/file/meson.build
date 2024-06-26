file_inc = include_directories('include')

file_lib = library(
    'file_lib',
    'file.c',
    include_directories: file_inc,
    dependencies: [
        common_dep,
    ],
)

file_dep = declare_dependency(
    link_with: file_lib,
    include_directories: file_inc,
)