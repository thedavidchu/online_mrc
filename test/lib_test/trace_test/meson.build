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

# I'm not going to push a random trace to github
fs = import('fs')
# HACK  I need this weird back-bending because my version of C's fopen
#       doesn't support relative paths.
test_trace = join_paths(meson.current_source_dir(), '../../../data/src2.bin')
if fs.exists(test_trace)
    test('trace_test', trace_test_exe, args: [test_trace])
else
    warning('could not find file: ' + test_trace)
endif