# Directory-wide include
reuse_stack_include = include_directories('include')

# Olken Reuse Stack
reuse_stack_lib = library('reuse_stack_lib',
    'reuse_stack.c',
    include_directories : [reuse_stack_include, tree_include],
    dependencies : glib_dep,
    link_with : [naive_tree_lib, sleator_tree_lib])
