phase_sampler_test_exe = executable(
    'phase_sampler_test_exe',
    'phase_sampler_test.c',
    include_directories: [mytester_include],
    dependencies: [
        common_dep,
        histogram_dep,
        phase_sampler_dep,
        uniform_random_dep,
    ],
)

test('phase_sampler_test', phase_sampler_test_exe)