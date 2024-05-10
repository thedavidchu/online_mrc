# NOTE  I don't know why MurmurHash3 is being uncooperative, but it has
#       something to do with the rotl functions...

hash_inc = include_directories('include')

murmur_hash_3_lib = library(
    'murmur_hash_3_lib',
    'MurmurHash3.c',
    include_directories: hash_inc,
)

# My wrapper around the MurmurHash3 functions. I don't like the name because it
# doesn't match the functions inside.
# NOTE  Meson complains about libraries without sources.
my_murmur_hash_3_dep = declare_dependency(
    link_with: [
        murmur_hash_3_lib,
    ],
    include_directories: hash_inc,
    dependencies: [
        common_dep,
    ],
)

splitmix64_lib = library(
    'splitmix64_lib',
    'splitmix64.c',
    include_directories: hash_inc,
)

hash_dep = declare_dependency(
    link_with: [
        murmur_hash_3_lib,
        splitmix64_lib,
    ],
    include_directories: hash_inc,
    dependencies: [
        my_murmur_hash_3_dep,
    ],
)