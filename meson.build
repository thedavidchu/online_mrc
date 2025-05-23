project(
    'online_mrc',
    'c',
    'cpp',
    version: '0.0.0',
    license: 'none',
    # For C, it peaked in 1999.
    # For C++, the more modern, the better for me <3
    default_options: [
        'c_std=gnu11',
        # C++20 supports barriers and other synchronization primitives.
        'cpp_std=c++20',
        # This is equivalent to "-Wall -Werror -Wextra -Wpedantic" according to
        # this: https://mesonbuild.com/Builtin-options.html#details-for-warning_level
        'warning_level=3',
        # I link the libraries statically (default is dynamic) because I want to
        # see them when I use gprof.
        'default_library=static',
        # Build with the following options:
        # 1. 'debug' for -O0 -g
        # 2. 'debugoptimized' for -O2 -g
        # 3. 'release' for -O3
        'buildtype=debugoptimized',
        # Enable link-time optimizations
        'b_lto=false',
        # Disable profile guided optimization
        'b_pgo=off',
        'b_pie=false',
    ],
)

# For some reason, the <math.h> library needs to be added to the compiler
# separately in Linux. Source: https://mesonbuild.com/howtox.html#add-math-library-lm-portably
cc = meson.get_compiler('c', native: true)
math_dep = cc.find_library('m', required: false)
thread_dep = dependency('threads')

# We require the version to be newer than 1.81.0 so that it includes
# Boost's 'boost/unordered/unordered_flat_map.hpp' file.
boost_dep = dependency('boost', version: '>=1.81.0')

glib_dep = dependency(
    'glib-2.0',
    default_options: [
        # This isn't my code, so it's not like I can fix anything here! I turn
        # the warnings down to 3 so that -Wpadded is not turned on here.
        'warning_level=3',
    ],
)

# NOTE  gprof cannot be used with valgrind! Source:
#       https://stackoverflow.com/questions/2146082/valgrind-profiling-timer-expired
use_gprof = false
if use_gprof
    add_project_arguments('-pg', '-no-pie', language: ['c', 'cpp'])
    # NOTE  I have no idea why this is
    add_project_link_arguments('-pg', '-no-pie', language: ['c', 'cpp'])
endif

analyze_time_intervals = false
if analyze_time_intervals
    add_project_arguments('-DINTERVAL_STATISTICS', language: ['c', 'cpp'])
endif

analyze_thresholds = false
if analyze_thresholds
    add_project_arguments('-DTHRESHOLD_STATISTICS', language: ['c', 'cpp'])
endif

profile_statistics = false
if profile_statistics
    add_project_arguments('-DPROFILE_STATISTICS', language: ['c', 'cpp'])
endif

fs = import('fs')
# Define the test trace
# HACK  I need this weird back-bending because my version of C's fopen
#       doesn't support relative paths.
test_trace = join_paths(meson.current_source_dir(), 'data/src2.bin')
if not fs.exists(test_trace)
    warning('could not find file: ' + test_trace)
endif

# CMake Subproject: 1a1a11a/libCacheSim
cmake = import('cmake')
lib_cache_sim_opt = cmake.subproject_options()
lib_cache_sim_opt.set_override_option('warning_level', '0')
lib_cache_sim_subproject = cmake.subproject(
    'libCacheSim',
    options: lib_cache_sim_opt,
)

lib_cache_sim_dep = lib_cache_sim_subproject.dependency('libCacheSim')

subdir('src')
subdir('test')
subdir('bench')