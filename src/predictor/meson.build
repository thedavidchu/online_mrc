predictor_inc = include_directories('include')

list_dep = declare_dependency(
    link_with: library(
        'list_lib',
        'list.cpp',
        include_directories: predictor_inc,
        dependencies: [
            common_dep,
            cpp_cache_dep,
        ],
    ),
    include_directories: predictor_inc,
    dependencies: [
        common_dep,
    ],
)

list_exe = executable(
    'list_exe',
    ['test_list.cpp'],
    dependencies: [
        common_dep,
        list_dep,
    ],
)

lru_ttl_cache_exe = executable(
    'lru_ttl_cache_exe',
    'lru_ttl_cache.cpp',
    include_directories: predictor_inc,
    dependencies: [cpp_cache_dep, list_dep],
)

test_lru_ttl_cache_exe = executable(
    'test_lru_ttl_cache_exe',
    'test_lru_ttl_cache.cpp',
    include_directories: predictor_inc,
    dependencies: [cpp_cache_dep, list_dep],
)

predictor_exe = executable(
    'predictor_exe',
    'predictor.cpp',
    include_directories: cache_inc,
    dependencies: [
        common_dep,
        io_dep,
        list_dep,
        trace_dep,
        cache_statistics_dep,
        cpp_cache_dep,
    ],
)

test_lifetime_thresholds_exe = executable(
    'test_lifetime_thresholds',
    'test_lifetime_thresholds.cpp',
    include_directories: predictor_inc,
    dependencies: [
        common_dep,
    ],
)

test('list_test', list_exe)
test('test_lifetime_thresholds', test_lifetime_thresholds_exe)
test('test_lru_ttl_cache', test_lru_ttl_cache_exe)