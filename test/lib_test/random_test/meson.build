random_test_include = include_directories('include')

random_test_exe = executable(
    'random_test_exe',
    'random_test.cpp',
    include_directories: [mytester_include, random_test_include],
    dependencies: [zipfian_random_dep, uniform_random_dep, common_dep],
)

test('random_test', random_test_exe)
