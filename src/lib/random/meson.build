random_include = include_directories('include')

# For some reason, the <math.h> library needs to be added to the compiler
# separately in Linux. Source: https://mesonbuild.com/howtox.html#add-math-library-lm-portably
cc = meson.get_compiler('c')
math_dep = cc.find_library('m', required : false)

uniform_random_lib = library('uniform_random_lib',
    'uniform_random.c',
    dependencies : math_dep,
    include_directories : random_include)

zipfian_random_lib = library('zipfian_random_lib',
    'zipfian_random.c',
    dependencies : math_dep,
    include_directories : random_include,
    link_with : uniform_random_lib)
