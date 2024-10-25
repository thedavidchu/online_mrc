print_trace_exe = executable(
    'print_trace_exe',
    'print_trace.c',
    dependencies: [
        common_dep,
        glib_dep,
        file_dep,
        io_dep,
        trace_dep,
    ],
)

test(
    'print_trace_exe',
    print_trace_exe,
    args: ['-i', '../data/src2.bin'],
)