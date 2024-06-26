interval_olken_inc = include_directories('interval/include')

interval_olken_lib = library(
    'interval_olken_lib',
    'interval/interval_olken.c',
    include_directories: interval_olken_inc,
    dependencies: [
        basic_tree_dep,
        common_dep,
        file_dep,
        lookup_dep,
        sleator_tree_dep,
        trace_dep,
        tree_dep,
    ],
)

interval_olken_dep = declare_dependency(
    link_with: interval_olken_lib,
    include_directories: interval_olken_inc,
    dependencies: [
        lookup_dep,
        tree_dep,
    ],
)

analyzer_exe = executable(
    'analyzer_exe',
    'interval/analyzer.c',
    include_directories: interval_olken_inc,
    dependencies: [
        common_dep,
        glib_dep,
        interval_olken_dep,
        file_dep,
        shards_dep,
        trace_dep,
    ],
)

test(
    'analyzer_test',
    analyzer_exe,
    args: ['-i', test_trace, '-o', 'output.bin', '-f', 'Kia'],
)