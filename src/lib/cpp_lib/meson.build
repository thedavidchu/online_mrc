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