fixed_size_shards_include = include_directories('include',
    '../../lib/common_headers/include',
    '../../lib/hash/include',
    '../../lib/histogram/include',
    '../../lib/priority_queue/include',
    )

fixed_size_shards_lib = library('fixed_size_shards_lib',
    'fixed_size_shards.c',
    include_directories : [
        common_headers_include,
        hash_include,
        histogram_include,
        priority_queue_include,
        tree_include,
        fixed_size_shards_include,
    ],
    dependencies : glib_dep,
    link_with : [
        naive_tree_lib,
        sleator_tree_lib,
        splitmix64_lib,
        splay_priority_queue_lib,
        basic_histogram_lib,
    ])
