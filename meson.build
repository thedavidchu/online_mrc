project('online_mrc', 'c', 'cpp',
    version : '0.0.0',
    license : 'none',
    # For C++, the more modern, the better for me <3
    # For C, it peaked in 1999.
    default_options : ['c_st=c99', 'cpp_std=c++17'])
glib_dep = dependency('glib-2.0')

subdir('src')

executable('demo', 'main.c',
    dependencies : glib_dep)

