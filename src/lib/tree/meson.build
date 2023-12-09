# Directory-wide include
tree_include = include_directories('include')

# Naive Tree
naive_tree_lib = library('naive_tree_lib',
    'naive_tree.c',
    include_directories : tree_include)

# Sleator's Splay Tree
sleator_tree_lib = library('sleator_tree_lib',
    'sleator_tree.c',
    include_directories : tree_include,
    link_with : naive_tree_lib)
