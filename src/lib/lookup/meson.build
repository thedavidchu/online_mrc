lookup_inc = include_directories('include', '.')

lookup_lib = library(
    'lookup_lib',
    [
        'hash_table.c',
        'dictionary.c',
        'parallel_hash_table.c',
        'parallel_list.c',
        'evicting_hash_table.c',
        'k_hash_table.c',
        'boost_hash_table.cpp',
    ],
    dependencies: [
        array_dep,
        boost_dep,
        common_dep,
        glib_dep,
        math_dep,
        thread_dep,
        hash_dep,
    ],
    include_directories: lookup_inc,
)

lookup_dep = declare_dependency(
    link_with: lookup_lib,
    dependencies: [
        common_dep,
        glib_dep,
        hash_dep,
        math_dep,
        thread_dep,
    ],
    include_directories: lookup_inc,
)