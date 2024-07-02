hyperloglog_test_exe = executable(
    'hyperloglog_test_exe',
    'hyperloglog_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        lookup_dep,
        shards_dep,
        trace_dep,
        uniform_random_dep,
        zipfian_random_dep,
    ],
)

test(
    'hyperloglog_test',
    hyperloglog_test_exe,
    args: ['-t', test_trace, '-u', '-z', '--cleanup'],
)