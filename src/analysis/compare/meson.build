compare_mrc_exe = executable(
    'compare_mrc_exe',
    'compare_mrc.c',
    dependencies: [
        common_dep,
        file_dep,
        miss_rate_curve_dep,
    ],
)

test('compare_mrc_test', compare_mrc_exe, args: '-h')