saturation_arithmetic_test_exe = executable(
    'saturation_arithmetic_test_exe',
    'saturation_arithmetic_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        glib_dep,
    ],
)

test('saturation_arithmetic_test', saturation_arithmetic_test_exe)