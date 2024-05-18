timer_inc = include_directories('include')

timer_lib = library(
    'timer_lib',
    'timer.c',
    include_directories: timer_inc,
    dependencies: [
        common_dep,
    ],
)

timer_dep = declare_dependency(
    link_with: timer_lib,
    include_directories: timer_inc,
)