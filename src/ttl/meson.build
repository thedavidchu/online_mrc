cache_inc = include_directories('include')

ttl_exe = executable(
    'ttl_exe',
    'ttl.cpp',
    include_directories: cache_inc,
    link_with: [],
    dependencies: [
        boost_dep,
        common_dep,
        glib_dep,
        io_dep,
        histogram_dep,
        priority_queue_dep,
        file_dep,
        timer_dep,
        trace_dep,
    ],
)