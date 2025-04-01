cardinality_analyzer_exe = executable(
    'cardinality_analyzer_exe',
    'cardinality_analyzer.c',
    include_directories: [],
    dependencies: [
        common_dep,
        olken_dep,
        glib_dep,
        lookup_dep,
        shards_dep,
        trace_dep,
        uniform_random_dep,
        zipfian_random_dep,
    ],
)

test(
    'cardinality_analyzer_test',
    cardinality_analyzer_exe,
    args: ['-t', test_trace, '-u', '-z', '--cleanup'],
)