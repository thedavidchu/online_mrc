trace_test_exe = executable(
    'trace_test_exe',
    'trace_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        trace_dep,
    ],
)

fs = import('fs')
if fs.exists(test_trace)
    test('trace_test', trace_test_exe, args: [test_trace])
else
    warning('could not find file: ' + test_trace)
endif