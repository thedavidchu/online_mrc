sampler_inc = include_directories('include')

phase_sampler_lib = library(
    'phase_sampler',
    'phase_sampler.c',
    include_directories: sampler_inc,
    dependencies: [
        common_dep,
        glib_dep,
        histogram_dep,
    ],
)

phase_sampler_dep = declare_dependency(
    link_with: phase_sampler_lib,
    include_directories: sampler_inc,
    dependencies: [
        glib_dep,
        histogram_dep,
    ],
)