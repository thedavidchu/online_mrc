test_cache_predictive_metadata_exe = executable(
    'test_cache_predictive_metadata_exe',
    'test_cache_predictive_metadata.cpp',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        cpp_lib_dep,
    ],
)

test_enumerate_exe = executable(
    'test_enumerate_exe',
    'test_enumerate.cpp',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        cpp_lib_dep,
    ],
)

test('test_cache_predictive_metadata', test_cache_predictive_metadata_exe)
test('test_enumerate', test_enumerate_exe)