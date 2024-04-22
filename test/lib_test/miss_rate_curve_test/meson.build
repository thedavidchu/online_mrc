miss_rate_curve_test_exe = executable(
    'miss_rate_curve_test_exe',
    'miss_rate_curve_test.c',
    include_directories: [
        mytester_include,
        miss_rate_curve_include,
    ],
    dependencies: [
        common_dep,
        basic_histogram_dep,
        fractional_histogram_dep,
        glib_dep,
        basic_miss_rate_curve_dep,
    ],
)

test('miss_rate_curve_test', miss_rate_curve_test_exe)
