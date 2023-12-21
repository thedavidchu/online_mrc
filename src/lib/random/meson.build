random_include = include_directories('include')

uniform_random_lib = library('uniform_random_lib',
    'uniform_random.c',
    dependencies : math_dep,
    include_directories : random_include)

zipfian_random_lib = library('zipfian_random_lib',
    'zipfian_random.c',
    dependencies : math_dep,
    include_directories : random_include,
    link_with : uniform_random_lib)
