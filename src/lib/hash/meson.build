# NOTE  I don't know why MurmurHash3 is being uncooperative, but it has
#       something to do with the rotl functions...
if true
    murmur_hash_3_dep = declare_dependency(
        link_with: library(
            'murmur_hash_3_lib',
            'MurmurHash3.c',
            include_directories: include_directories('include'),
        ),
        include_directories: include_directories('include'),
    )
endif

my_murmur_hash_3_dep = declare_dependency(
    include_directories: include_directories('include'),
    dependencies: [
        common_dep,
        murmur_hash_3_dep,
    ],
)

splitmix64_dep = declare_dependency(
    link_with: library(
        'splitmix64_lib',
        'splitmix64.c',
        include_directories: include_directories('include'),
    ),
    include_directories: include_directories('include'),
)
