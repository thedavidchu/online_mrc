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
        average_eviction_time_dep,
        evicting_map_dep,
        common_dep,
        fixed_rate_shards_dep,
        fixed_size_shards_dep,
        glib_dep,
        goel_quickmrc_dep,
        io_dep,
        miss_rate_curve_dep,
        olken_dep,
        quickmrc_dep,
        timer_dep,
        trace_dep,
    ],
)