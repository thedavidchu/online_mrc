lookup_dep = declare_dependency(
    link_with: library(
        'lookup_lib',
        [
            'hash_table.c',
            'parallel_hash_table.c',
            'parallel_list.c',
            'evicting_hash_table.c',
        ],
        dependencies: [
            common_dep,
            glib_dep,
            math_dep,
            thread_dep,
            hash_dep,
        ],
        include_directories: include_directories('include'),
    ),
    dependencies: [
        common_dep,
        glib_dep,
        hash_dep,
        math_dep,
        thread_dep,
    ],
    include_directories: include_directories('include'),
)