interval_inc = include_directories('include')

interval_olken_lib = library(
    'interval_olken_lib',
    'interval_olken.c',
    include_directories: interval_inc,
    dependencies: [
        common_dep,
        file_dep,
        interval_statistics_dep,
        math_dep,
        olken_dep,
        trace_dep,
    ],
)

interval_dep = declare_dependency(
    link_with: [
        interval_olken_lib,
    ],
    include_directories: interval_inc,
    dependencies: [
        interval_statistics_dep,
    ],
)

interval_test_exe = executable(
    'interval_test_exe',
    'interval_test.c',
    include_directories: interval_inc,
    dependencies: [
        common_dep,
        glib_dep,
        interval_dep,
        file_dep,
        shards_dep,
        trace_dep,
    ],
)

interval_analyzer_exe = executable(
    'interval_analyzer_exe',
    'interval_analyzer.c',
    include_directories: interval_inc,
    dependencies: [
        common_dep,
        glib_dep,
        interval_dep,
        file_dep,
        shards_dep,
        trace_dep,
        # Test with evicting map to debug
        evicting_map_dep,
    ],
)

test('interval_test', interval_test_exe, args: [test_trace])

test(
    'interval_analyzer_test',
    interval_analyzer_exe,
    args: ['-i', test_trace, '-o', 'output.bin', '-f', 'Kia', '--cleanup'],
)