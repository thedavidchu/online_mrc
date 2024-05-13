# This is for targets that only need the data structures for the trees
tree_dep = declare_dependency(
    include_directories: [
        include_directories('include'),
    ],
    dependencies: [
        common_dep,
    ]
)

# Naive Tree
basic_tree_dep = declare_dependency(
    link_with: library(
        'basic_tree_lib',
        'basic_tree.c',
        dependencies: tree_dep,
    ),
    include_directories: [
        include_directories('include'),
    ],
    dependencies: common_dep,
)

# Sleator's Splay Tree
sleator_tree_dep = declare_dependency(
    link_with: library(
        'sleator_tree_lib',
        'sleator_tree.c',
        dependencies: [
            tree_dep,
            basic_tree_dep,
        ],
    ),
    include_directories: [
        include_directories('include'),
    ],
    dependencies: common_dep,
)
