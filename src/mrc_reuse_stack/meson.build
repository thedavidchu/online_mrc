# Directory-wide include
reuse_stack_include = include_directories('include',
    '../lib/common_headers/include',
    '../lib/hash/include',
    '../lib/histogram/include',
    '../lib/priority_queue/include',
    )

# Reuse Stack Libraries
olken_reuse_stack_lib = library('olken_reuse_stack_lib',
    'olken.c',
    include_directories : [
        common_headers_include,
        histogram_include,
        tree_include,
        reuse_stack_include,
    ],
    dependencies : glib_dep,
    link_with : [
        naive_tree_lib,
        basic_histogram_lib,
        sleator_tree_lib,
    ])

fixed_size_shards_lib = library('fixed_size_shards_lib',
    'fixed_size_shards.c',
    include_directories : [
        common_headers_include,
        hash_include,
        histogram_include,
        priority_queue_include,
        tree_include,
        reuse_stack_include,
    ],
    dependencies : glib_dep,
    link_with : [
        naive_tree_lib,
        sleator_tree_lib,
        splitmix64_lib,
        splay_priority_queue_lib,
        basic_histogram_lib,
    ])
