# Directory-wide include
histogram_include = include_directories('include')

# Basic Histogram
basic_histogram_lib = library('basic_histogram_lib',
    'basic_histogram.c',
    include_directories : [histogram_include])

fractional_histogram_lib = library('fractional_histogram_lib',
    'fractional_histogram.c',
    include_directories : [
        common_headers_include,
        histogram_include,
])
