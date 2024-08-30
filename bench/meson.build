subdir('hash_test')
subdir('lookup_test')
subdir('mrc_test')

fast_slow_path_performance_test = executable(
    'fast_slow_path_performance_test',
    'fast_slow_path_performance_test.c',
    link_with: [
        trace_runner_lib,
    ],
    dependencies: [
        common_dep,
        hash_dep,
        run_dep,
        trace_dep,
    ],
)

test('fast_slow_path_performance_test', fast_slow_path_performance_test)