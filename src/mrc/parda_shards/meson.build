fixed_rate_shards_include = include_directories('include',
    'parda')

# This is the include directory for parda's internals
private_parda_include = include_directories('parda')

# I label this as the Parda implementation, because I am expecting PARDA to be
# slow (because of the string conversions). This way, I can easily deprecate it.
parda_fixed_rate_shards_lib = library('parda_fixed_rate_shards_lib',
    'parda_fixed_rate_shards.c',
    'parda/narray.c', 'parda/parda.c', 'parda/splay.c', 'parda/process_args.c',
    include_directories : [
        common_headers_include,
        fixed_rate_shards_include,
        hash_include,
        private_parda_include,
    ],
    dependencies : glib_dep,
    link_with : [
        splitmix64_lib,
    ],
)
