# random_include = include_directories('include')

uniform_random_dep = declare_dependency(
    link_with: library(
        'uniform_random_lib',
        'uniform_random.c',
        dependencies: math_dep,
        include_directories: include_directories('include'),
    ),
    include_directories: include_directories('include'),
)

zipfian_random_dep = declare_dependency(
    link_with: library(
        'zipfian_random_lib',
        'zipfian_random.c',
        dependencies: [math_dep, uniform_random_dep],
        include_directories: include_directories('include'),
    ),
    include_directories: include_directories('include'),
)
