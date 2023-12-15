# Directory-wide include
priority_queue_include = include_directories('include', '../common_headers/include', '../hash/include')

splay_priority_queue_lib = library('splay_priority_queue_lib',
    'splay_priority_queue.c',
    include_directories : [common_headers_include, hash_include, priority_queue_include],
    link_with : splitmix64_lib)
