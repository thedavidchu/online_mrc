project('online_mrc', 'c', 'cpp',
    version : '0.0.0',
    license : 'none',
    # For C, it peaked in 1999.
    # For C++, the more modern, the better for me <3
    default_options : [
        'c_st=c99',
        'cpp_std=c++17',
        # This is equivalent to "-Wall -Werror -Wextra -Wpedantic" according to
        # this: https://mesonbuild.com/Builtin-options.html#details-for-warning_level
        'warning_level=everything',
    ])
# add_project_arguments('-Wall', '-Werror', '-Wextra', '-Wpedantic', language : 'c')
glib_dep = dependency('glib-2.0')

subdir('src')
subdir('test')
