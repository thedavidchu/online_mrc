cpp_struct_inc = include_directories('include')

hash_list_dep = declare_dependency(
    link_with: library(
        'hash_list_lib',
        'hash_list.cpp',
        include_directories: cpp_struct_inc,
        dependencies: [
            common_dep,
        ],
    ),
    include_directories: cpp_struct_inc,
    dependencies: [
        common_dep,
    ],
)

cpp_struct_dep = declare_dependency(
    include_directories: cpp_struct_inc,
    dependencies: [
        hash_list_dep,
    ],
)