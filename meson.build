project('online_mrc', 'c', 'cpp',
    version : '0.0.0',
    license : 'none',
    # For C, it peaked in 1999.
    # For C++, the more modern, the better for me <3
    default_options : [
        'c_std=c99',
        'cpp_std=c++17',
        # This is equivalent to "-Wall -Werror -Wextra -Wpedantic" according to
        # this: https://mesonbuild.com/Builtin-options.html#details-for-warning_level
        'warning_level=everything',
        # I link the libraries statically (default is dynamic) because I want to
        # see them when I use gprof.
        'default_library=static',
    ])
glib_dep = dependency('glib-2.0')

use_gprof = true
if use_gprof
    add_project_arguments('-pg', '-no-pie', language : 'c')
    # NOTE  I have no idea why this is
    add_project_link_arguments('-pg', '-no-pie', language : 'c')
endif

subdir('src')
subdir('test')
