compare_mrc_exe = executable(
    'compare_mrc_exe',
    'compare_mrc.c',
    dependencies: [
        common_dep,
        file_dep,
        miss_rate_curve_dep,
    ],
)

generate_oracle_exe = executable(
    'generate_oracle_exe',
    'generate_oracle.c',
    dependencies: [
        common_dep,
        file_dep,
        histogram_dep,
        miss_rate_curve_dep,
        olken_dep,
        timer_dep,
        trace_dep,
    ],
)

test(
    'generate_oracle_test',
    generate_oracle_exe,
    args: [
        '-i', test_trace,
        '-f', 'Kia',
        '-h', 'generate_oracle_test-histogram.bin',
        '-m', 'generate_oracle_test-mrc.bin',
        '--cleanup',
    ],
)

generate_multi_mrc_exe = executable(
    'generate_multi_mrc_exe',
    'generate_multi_mrc.c',
    include_directories: [include_directories('.')],
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
    'generate_multi_mrc_olken_test',
    generate_multi_mrc_exe,
    args: [
        '-i', 'zipf',
        '-l', '10000',
        '-o', 'Olken(mrc=generate_multi_mrc_olken_test-olken-mrc.bin,hist=generate_multi_mrc_olken_test-olken-hist.bin)',
        '--cleanup',
    ],
)

test(
    'generate_multi_mrc_test',
    generate_multi_mrc_exe,
    args: [
        '-i', 'zipf',
        '-l', '10000',
        '-o', 'Olken(mrc=generate_multi_mrc_test-olken-mrc.bin,hist=generate_multi_mrc_test-olken-hist.bin,bin_size=1024,mode=realloc)',
        '-r', 'Fixed-Rate-SHARDS(mrc=generate_multi_mrc_test-frs-mrc.bin,hist=generate_multi_mrc_test-frs-hist.bin,sampling=1e-3,num_bins=1024,bin_size=1024,mode=realloc,adj=true)',
        '-r', 'Fixed-Size-SHARDS(mrc=generate_multi_mrc_test-fss-mrc.bin,hist=generate_multi_mrc_test-fss-hist.bin,sampling=1e-1,num_bins=1024,bin_size=1024,mode=realloc,adj=false)',
        '-r', 'Evicting-Map(mrc=generate_multi_mrc_test-emap-mrc.bin,hist=generate_multi_mrc_test-emap-hist.bin,sampling=1e-1,num_bins=1024,bin_size=1024,mode=realloc,adj=false)',
        '--cleanup',
    ],
)

generate_mrc_exe = executable(
    'generate_mrc_exe',
    'generate_mrc.c',
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

################################################################################
### GENERATE MRC TESTS
################################################################################
test(
    'generate_mrc_olken_test',
    generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Olken',
        '-o', 'olken-mrc.bin',
        '--cleanup',
    ],
)
test(
    'generate_mrc_fixed_rate_shards_test',
    generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Fixed-Rate-SHARDS',
        '-o', 'fixed-rate-shards-mrc.bin',
        '--cleanup',
    ],
)
test(
    'generate_mrc_fixed_rate_shards_adj_test',
    generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Fixed-Rate-SHARDS-Adj',
        '-o', 'fixed-rate-shards-adj-mrc.bin',
        '--cleanup',
    ],
)
test(
    'generate_mrc_fixed_size_shards_test',
    generate_mrc_exe,
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
    'generate_mrc_quickmrc_test',
    generate_mrc_exe,
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
    'generate_mrc_goel_quickmrc_test',
    generate_mrc_exe,
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
    'generate_mrc_evicting_map_test',
    generate_mrc_exe,
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
    'generate_mrc_aet_test',
    generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Average-Eviction-Time',
        '-o', 'aet-mrc.bin',
        '--cleanup',
    ],
)
test(
    'generate_mrc_their_aet_test',
    generate_mrc_exe,
    args: [
        '-i', 'zipf',
        '-n', '10000',
        '-a', 'Their-Average-Eviction-Time',
        '-o', 'their-aet-mrc.bin',
        '--cleanup',
    ],
)