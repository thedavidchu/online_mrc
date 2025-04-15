subdir('cardinality')
subdir('compare')
subdir('interval')
subdir('read_write')
subdir('text')

analyze_trace_exe = executable(
    'analyze_trace_exe',
    'analyze_trace.cpp',
    dependencies: [
        cpp_cache_dep,
    ],
)
analyze_clients_exe = executable(
    'analyze_clients_exe',
    'analyze_clients.cpp',
    dependencies: [
        cpp_cache_dep,
    ],
)