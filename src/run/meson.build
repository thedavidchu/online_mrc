run_inc = include_directories('include')

runner_arguments_lib = library(
    'runner_arguments_lib',
    'runner_arguments.c',
    include_directories: run_inc,
    dependencies: [
        common_dep,
        histogram_dep,
        lookup_dep,
    ],
)

run_oracle_lib = library(
    'run_oracle_lib',
    'run_oracle.c',
    link_with: runner_arguments_lib,
    include_directories: run_inc,
    dependencies: [
        common_dep,
        histogram_dep,
        io_dep,
        miss_rate_curve_dep,
        olken_dep,
        trace_dep,
    ],
)

trace_runner_lib = library(
    'trace_runner_lib',
    'trace_runner.c',
    include_directories: run_inc,
    dependencies: [
        average_eviction_time_dep,
        evicting_map_dep,
        evicting_quickmrc_dep,
        common_dep,
        shards_dep,
        glib_dep,
        goel_quickmrc_dep,
        file_dep,
        miss_rate_curve_dep,
        olken_dep,
        quickmrc_dep,
        timer_dep,
        trace_dep,
    ],
)

run_dep = declare_dependency(
    include_directories: run_inc,
    link_with: [
        runner_arguments_lib,
        trace_runner_lib,
    ],
    dependencies: [
        histogram_dep,
        lookup_dep,
        trace_dep,
    ],
)

generate_mrc_exe = executable(
    'generate_mrc_exe',
    'generate_mrc.c',
    link_with: [
        runner_arguments_lib,
        run_oracle_lib,
        trace_runner_lib,
    ],
    include_directories: run_inc,
    dependencies: [
        average_eviction_time_dep,
        evicting_map_dep,
        common_dep,
        shards_dep,
        glib_dep,
        goel_quickmrc_dep,
        file_dep,
        miss_rate_curve_dep,
        olken_dep,
        quickmrc_dep,
        timer_dep,
        trace_dep,
    ],
)

test(
    'generate_mrc_empty_test',
    generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-l', '10000',
        '-r', 'Olken()',
        '--cleanup',
    ],
)

test(
    'generate_mrc_zipf_test',
    generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-l', '10000',
        '-o', 'Olken(mrc=generate_mrc_zipf_test-olken-mrc.bin,hist=generate_mrc_zipf_test-olken-hist.bin)',
        '--cleanup',
    ],
)

test(
    'generate_mrc_trace_test',
    generate_mrc_exe,
    args: [
        '-i', test_trace,
        '-f', 'Kia',
        '-o', 'Olken(mrc=generate_mrc_trace_test-mrc.bin,hist=generate_mrc_trace_test-histogram.bin)',
        '--cleanup',
    ],
)

test(
    'generate_mrc_trace_dictionary_test',
    generate_mrc_exe,
    args: [
        '-i', test_trace,
        '-f', 'Kia',
        '-r', 'Evicting-Map(a=A,b=B,c=C)',
        '-r', 'Fixed-Size-SHARDS(a=A,b=B,c=C)',
        '--cleanup',
    ],
)

test(
    'generate_mrc_main_test',
    generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-l', '1000000',
        '-o', 'Olken(mrc=generate_mrc_main_test-olken-mrc.bin,hist=generate_mrc_main_test-olken-hist.bin,bin_size=1024,mode=realloc)',
        '-r', 'Fixed-Rate-SHARDS(mrc=generate_mrc_main_test-frs-mrc.bin,hist=generate_mrc_main_test-frs-hist.bin,sampling=1e-3,num_bins=1024,bin_size=1024,mode=realloc,adj=true)',
        '-r', 'Fixed-Size-SHARDS(mrc=generate_mrc_main_test-fss-mrc.bin,hist=generate_mrc_main_test-fss-hist.bin,sampling=1e-1,num_bins=1024,bin_size=1024,max_size=8192,mode=realloc,adj=false)',
        '-r', 'Evicting-Map(mrc=generate_mrc_main_test-emap-mrc.bin,hist=generate_mrc_main_test-emap-hist.bin,sampling=1e-1,num_bins=1024,bin_size=1024,max_size=8192,mode=realloc,adj=false)',
        '--cleanup',
    ],
)

################################################################################
### GENERATE MRC TESTS
########################
deprecated_generate_mrc_exe = executable(
    'deprecated_generate_mrc_exe',
    'deprecated_generate_mrc.c',
    include_directories: [],
    dependencies: [
        average_eviction_time_dep,
        evicting_map_dep,
        common_dep,
        shards_dep,
        glib_dep,
        goel_quickmrc_dep,
        file_dep,
        miss_rate_curve_dep,
        olken_dep,
        quickmrc_dep,
        timer_dep,
        trace_dep,
    ],
)
########################################################
test(
    'deprecated_generate_mrc_olken_test',
    deprecated_generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Olken',
        '-o', 'olken-mrc.bin',
        '--cleanup',
    ],
)
test(
    'deprecated_generate_mrc_fixed_rate_shards_test',
    deprecated_generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Fixed-Rate-SHARDS',
        '-o', 'fixed-rate-shards-mrc.bin',
        '--cleanup',
    ],
)
test(
    'deprecated_generate_mrc_fixed_rate_shards_adj_test',
    deprecated_generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Fixed-Rate-SHARDS-Adj',
        '-o', 'fixed-rate-shards-adj-mrc.bin',
        '--cleanup',
    ],
)
test(
    'deprecated_generate_mrc_fixed_size_shards_test',
    deprecated_generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Fixed-Size-SHARDS',
        '-s', '0.1',
        '-o', 'fixed-size-shards-mrc.bin',
        '--cleanup',
    ],
)
test(
    'deprecated_generate_mrc_quickmrc_test',
    deprecated_generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'QuickMRC',
        '-s', '1.0',
        '-o', 'quickmrc-mrc.bin',
        '--cleanup',
    ],
)
test(
    'deprecated_generate_mrc_goel_quickmrc_test',
    deprecated_generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Goel-QuickMRC',
        '-s', '1.0',
        '-o', 'goel-quickmrc-mrc.bin',
        '--cleanup',
    ],
)
test(
    'deprecated_generate_mrc_evicting_map_test',
    deprecated_generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Evicting-Map',
        '-s', '0.1',
        '-o', 'evicting-map-mrc.bin',
        '--cleanup',
    ],
)
test(
    'deprecated_generate_mrc_aet_test',
    deprecated_generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Average-Eviction-Time',
        '-o', 'aet-mrc.bin',
        '--cleanup',
    ],
)
test(
    'deprecated_generate_mrc_their_aet_test',
    deprecated_generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Their-Average-Eviction-Time',
        '-o', 'their-aet-mrc.bin',
        '--cleanup',
    ],
)