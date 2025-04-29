subdir('cardinality')
subdir('compare')
subdir('interval')
subdir('read_write')
subdir('text')

analyze_trace_exe = executable(
    'analyze_trace_exe',
    'analyze_trace.cpp',
    dependencies: [
        cpp_lib_dep,
    ],
)

analyze_clients_exe = executable(
    'analyze_clients_exe',
    'analyze_clients.cpp',
    dependencies: [
        cpp_lib_dep,
    ],
)

analyze_rw_exe = executable(
    'analyze_rw_exe',
    'analyze_rw.cpp',
    dependencies: [
        cpp_lib_dep,
    ],
)

analyze_ttls_exe = executable(
    'analyze_ttls_exe',
    'analyze_ttls.cpp',
    dependencies: [
        cpp_lib_dep,
    ],
)