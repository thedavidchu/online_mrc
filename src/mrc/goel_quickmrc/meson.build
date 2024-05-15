goel_quickmrc_inc = include_directories('include')

goel_quickmrc_lib = library(
    'goel_quickmrc_lib',
    [
        'goel_quickmrc.c',
        'quickmrc/qmrc/qmrc.c',
        'quickmrc/cache_simulation.c',
        'quickmrc/cache.c',
    ],
    include_directories: include_directories(
        'include',
        'quickmrc',
        'quickmrc/qmrc',
    ),
    dependencies: [
        common_dep,
        miss_rate_curve_dep,
    ],
    c_args: [
        '-DQMRC',
        '-DASSERT',
        # NOTE  This is so then we can inline the vectorized instructions.
        '-march=native',
        # HACK  Turn off all of the failing compiler flags...
        '-Wno-builtin-declaration-mismatch',
        '-Wno-implicit-function-declaration',
        '-Wno-pedantic',
        '-Wno-psabi',
        '-Wno-sign-compare',
        '-Wno-type-limits',
    ],
)

goel_quickmrc_dep = declare_dependency(
    link_with: goel_quickmrc_lib,
    include_directories: goel_quickmrc_inc,
    dependencies: [
        miss_rate_curve_dep,
    ],
)