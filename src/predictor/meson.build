predictor_inc = include_directories('include')

list_dep = declare_dependency(
    link_with: library(
        'list_lib',
        'list.cpp',
        include_directories: predictor_inc,
        dependencies: [
            common_dep,
        ],
    ),
    include_directories: predictor_inc,
    dependencies: [
        common_dep,
    ],
)

list_exe = executable(
    'list_exe',
    ['test_list.cpp'],
    dependencies: [
        common_dep,
        list_dep,
    ],
)

predictor_exe = executable(
    'predictor_exe',
    'predictor.cpp',
    include_directories: cache_inc,
    dependencies: [
        common_dep,
        io_dep,
        list_dep,
        trace_dep,
    ],
)

test('list_test', list_exe)