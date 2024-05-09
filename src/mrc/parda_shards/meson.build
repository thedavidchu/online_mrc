# I label this as the Parda implementation, because I am expecting PARDA to be
# slow (because of the string conversions). This way, I can easily deprecate it.
parda_fixed_rate_shards_dep = declare_dependency(
    link_with: library(
        'parda_fixed_rate_shards_lib',
        'parda_fixed_rate_shards.c',
        'parda/narray.c',
        'parda/parda.c',
        'parda/splay.c',
        'parda/process_args.c',
        include_directories: include_directories('include', 'parda'),
        dependencies: [
            common_dep,
            glib_dep,
            hash_dep,
        ],
    ),
    include_directories: include_directories('include', 'parda'),
)
