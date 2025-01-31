list_exe = executable(
    'list_exe',
    ['test_list.cpp', 'list.cpp'],
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
    ],
)

predictor_exe = executable(
    'predictor_exe',
    'predictor.cpp',
    include_directories: cache_inc,
    dependencies: [
        common_dep,
        io_dep,
        trace_dep,
    ],
)

test('list_test', list_exe)