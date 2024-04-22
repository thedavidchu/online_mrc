random_test_include = include_directories('include')

random_test_exe = executable('random_test_exe',
    'random_test.cpp',
    include_directories : [
        mytester_include,
        common_headers_include,
        random_include,
        random_test_include
    ],
    link_with : [uniform_random_lib, zipfian_random_lib])

test('random_test',
    random_test_exe)
