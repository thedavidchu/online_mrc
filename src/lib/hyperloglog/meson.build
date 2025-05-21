hyperloglog_dep = declare_dependency(
    include_directories: include_directories('include'),
    link_with: library(
        'hyperloglog_lib',
        'hyperloglog.cpp',
        include_directories: include_directories('include'),
        dependencies: [
            common_dep,
            cpp_lib_dep,
        ],
    ),
)