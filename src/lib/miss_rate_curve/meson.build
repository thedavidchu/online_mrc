miss_rate_curve_include = include_directories('include', '../common_headers/include')

basic_miss_rate_curve_lib = library('basic_miss_rate_curve_lib',
    'basic_miss_rate_curve.c',
    include_directories : [
        common_headers_include,
        histogram_include,
        miss_rate_curve_include,
    ],
    dependencies : glib_dep)
