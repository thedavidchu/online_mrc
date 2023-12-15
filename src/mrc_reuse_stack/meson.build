# Directory-wide include
reuse_stack_include = include_directories('include', '../lib/common_headers/include')

# Reuse Stack Libraries
olken_reuse_stack_lib = library('olken_reuse_stack_lib',
    'olken.c',
    include_directories : [common_headers_include, reuse_stack_include, tree_include],
    dependencies : glib_dep,
    link_with : [naive_tree_lib, sleator_tree_lib])
