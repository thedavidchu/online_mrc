sampling_inc = include_directories('include')

reservoir_sampling_lib = library(
    'reservoir_sampling_lib',
    'reservoir_sampling.c',
    include_directories: sampling_inc,
    dependencies: [
        common_dep,
        uniform_random_dep,
    ],
)

sampling_dep = declare_dependency(
    link_with: reservoir_sampling_lib,
    include_directories: sampling_inc,
    dependencies: [
        common_dep,
        uniform_random_dep,
    ],
)
