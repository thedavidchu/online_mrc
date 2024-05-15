goel_quickmrc_inc = include_directories('include')

goel_quickmrc_lib = library(
    'goel_quickmrc_lib',
    [
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
    ],
    c_args: [
        '-DQMRC',
        # NOTE  This is so then we can inline the vectorized instructions.
        '-march=native',
        # HACK  Turn off all of the failing compiler flags...
        '-Wno-builtin-declaration-mismatch',
        '-Wno-implicit-function-declaration',
        '-Wno-pedantic',
        '-Wno-psabi',
        '-Wno-sign-compare',
    ],
)

goel_quickmrc_dep = declare_dependency(
    link_with: goel_quickmrc_lib,
    include_directories: goel_quickmrc_inc,
)