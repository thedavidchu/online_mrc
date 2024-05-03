miss_rate_curve_test_exe = executable(
    'miss_rate_curve_test_exe',
    'miss_rate_curve_test.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        histogram_dep,
        fractional_histogram_dep,
        glib_dep,
        miss_rate_curve_dep,
    ],
)

test('miss_rate_curve_test', miss_rate_curve_test_exe)
