mytester_include = include_directories(
    'include',
)

# Calling the subdir after the include allows all of the test subdirectories to
# use the mytester_include.
subdir('lib_test')
subdir('mrc_test')

################################################################################
### GENERATE MRCs
################################################################################

generate_mrc_exe = executable(
    'generate_mrc_exe',
    'generate_mrc.c',
    include_directories: [
        mytester_include,
    ],
    dependencies: [
        common_dep,
        fixed_rate_shards_dep,
        fixed_size_shards_dep,
        glib_dep,
        miss_rate_curve_dep,
        olken_dep,
        trace_dep,
    ],
)
