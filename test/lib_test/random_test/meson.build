# Here are some notes on the sources of my code:
# * I copied the `.hpp` files from https://github.com/thedavidchu/merry_mem/blob/main/src/trace/
# * I took those from https://github.com/uoft-felis/felis/tree/master/benchmark/ycsb

random_test_exe = executable(
    'random_test_exe',
    'random_test.cpp',
    include_directories: [mytester_include, 'include'],
    dependencies: [zipfian_random_dep, uniform_random_dep, common_dep],
)

test('random_test', random_test_exe)