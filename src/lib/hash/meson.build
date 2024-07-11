# NOTE  I don't know why MurmurHash3 is being uncooperative, but it has
#       something to do with the rotl functions...

hash_inc = include_directories('include')

murmur_hash_3_lib = library(
    'murmur_hash_3_lib',
    'MurmurHash3.c',
    include_directories: hash_inc,
    c_args: [
        # MurmurHash3 uses fallthrough in the switch-case extensively!
        '-Wno-implicit-fallthrough',
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
        common_dep,
    ],
)