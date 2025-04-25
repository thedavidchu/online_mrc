cpp_cache_inc = include_directories('include')

cpp_lfu_cache_dep = declare_dependency(
    include_directories: cpp_cache_inc,
    link_with: library(
        'cpp_lfu_cache_lib',
        'lfu_cache.cpp',
        include_directories: cpp_cache_inc,
        dependencies: [
            cpp_struct_dep,
        ],
    ),
    dependencies: [
        cpp_struct_dep,
    ],
)

cpp_cache_dep = declare_dependency(
    include_directories: cpp_cache_inc,
    dependencies: [
        cpp_lfu_cache_dep,
    ],
)