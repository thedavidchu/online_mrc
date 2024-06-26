fixed_rate_shards_sampler_lib = library(
    'fixed_rate_shards_sampler_lib',
    'fixed_rate_shards_sampler.c',
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
        glib_dep,
        olken_dep,
        hash_dep,
        histogram_dep,
        priority_queue_dep,
        basic_tree_dep,
        sleator_tree_dep,
    ],
)

fixed_size_shards_sampler_lib = library(
    'fixed_size_shards_sampler_lib',
    'fixed_size_shards_sampler.c',
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
        glib_dep,
        hash_dep,
        priority_queue_dep,
        basic_tree_dep,
        sleator_tree_dep,
    ],
)

fixed_size_shards_lib = library(
    'fixed_size_shards_lib',
    'fixed_size_shards.c',
    include_directories: include_directories('include'),
    link_with: [
        fixed_size_shards_sampler_lib,
    ],
    dependencies: [
        basic_tree_dep,
        common_dep,
        glib_dep,
        histogram_dep,
        hash_dep,
        miss_rate_curve_dep,
        olken_dep,
        sleator_tree_dep,
        priority_queue_dep,
    ],
)

fixed_rate_shards_lib = library(
    'fixed_rate_shards_lib',
    'fixed_rate_shards.c',
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
        glib_dep,
        olken_dep,
        hash_dep,
        histogram_dep,
        priority_queue_dep,
        miss_rate_curve_dep,
        basic_tree_dep,
        sleator_tree_dep,
    ],
)

shards_dep = declare_dependency(
    link_with: [
        fixed_rate_shards_sampler_lib,
        fixed_size_shards_sampler_lib,
        fixed_rate_shards_lib,
        fixed_size_shards_lib,
    ],
    dependencies: [
        common_dep,
        glib_dep,
        histogram_dep,
        olken_dep,
        priority_queue_dep,
    ],
    include_directories: include_directories('include'),
)