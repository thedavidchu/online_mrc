mytester_include = include_directories(
    'include',
    '../src/lib/common_headers/include'
)

# Calling the subdir after the include allows all of the test subdirectories to
# use the mytester_include.
subdir('performance_test')
subdir('unit_test')
