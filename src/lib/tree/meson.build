# Directory-wide include
tree_include = include_directories('include')

# Naive Tree
basic_tree_lib = library('basic_tree_lib',
    'basic_tree.c',
    include_directories : tree_include)

# Sleator's Splay Tree
sleator_tree_lib = library('sleator_tree_lib',
    'sleator_tree.c',
    include_directories : tree_include,
    link_with : basic_tree_lib)
