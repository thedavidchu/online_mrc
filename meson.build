project(
    'online_mrc',
    'c',
    'cpp',
    version: '0.0.0',
    license: 'none',
    # For C, it peaked in 1999.
    # For C++, the more modern, the better for me <3
    default_options: [
        'c_std=c99',
        'cpp_std=c++17',
        # This is equivalent to "-Wall -Werror -Wextra -Wpedantic" according to
        # this: https://mesonbuild.com/Builtin-options.html#details-for-warning_level
        'warning_level=3',
        # I link the libraries statically (default is dynamic) because I want to
        # see them when I use gprof.
        'default_library=static',
        # Build with -O2 -g
        'buildtype=debug',
    ],
)

# For some reason, the <math.h> library needs to be added to the compiler
# separately in Linux. Source: https://mesonbuild.com/howtox.html#add-math-library-lm-portably
cc = meson.get_compiler('c')
math_dep = cc.find_library('m', required: false)

glib_dep = dependency(
    'glib-2.0',
    default_options: [
        # This isn't my code, so it's not like I can fix anything here! I turn
        # the warnings down to 3 so that -Wpadded is not turned on here.
        'warning_level=3',
    ],
)

use_gprof = true
if use_gprof
    add_project_arguments('-pg', '-no-pie', language: 'c')
    # NOTE  I have no idea why this is
    add_project_link_arguments('-pg', '-no-pie', language: 'c')
endif

subdir('src')
subdir('test')
