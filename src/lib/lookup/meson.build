lookup_dep = declare_dependency(
    link_with: library(
        'lookup_lib',
        [
            'parallel_hash_table.c',
            'parallel_list.c',
        ],
        dependencies: [
            common_dep,
            thread_dep,
            splitmix64_dep,
        ],
        include_directories: include_directories('include'),
    ),
    dependencies: [
        common_dep,
        thread_dep,
    ],
    include_directories: include_directories('include'),
)
