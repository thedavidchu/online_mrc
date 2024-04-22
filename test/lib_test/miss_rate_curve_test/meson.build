miss_rate_curve_test_exe = executable('miss_rate_curve_test_exe',
    'miss_rate_curve_test.c',
    include_directories : [
        common_headers_include,
        mytester_include,
        histogram_include,
        miss_rate_curve_include,
    ],
    dependencies : glib_dep,
    link_with : [
        basic_histogram_lib,
        fractional_histogram_lib,
        basic_miss_rate_curve_lib,
    ])

test('miss_rate_curve_test', miss_rate_curve_test_exe)
