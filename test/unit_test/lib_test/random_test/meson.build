random_test_include = include_directories('include')

random_test_exe = executable('random_test_exe',
    'random_test.cpp',
    include_directories : [mytester_include, random_include, random_test_include],
    link_with : [uniform_random_lib, zipfian_random_lib])

test('random_test',
    random_test_exe,
    # I disable the timeout. For some reason, it takes a really long time to do
    # 100x1000 random numbers (well it has to generate them twice, once for the
    # output and once for the oracle).
    timeout : 0)
