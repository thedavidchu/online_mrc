hash_include = include_directories('include')

# NOTE  I don't know why MurmurHash3 is being uncooperative, but it has
#       something to do with the rotl functions...
if false
    murmur_hash_3_lib = library('murmur_hash_3_lib',
        'MurmurHash3.c',
        include_directories : hash_include)
endif

splitmix64_lib = library('splitmix64_lib',
    'splitmix64.c',
    include_directories : hash_include)
