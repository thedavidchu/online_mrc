#pragma once

#include <stdint.h>

#include "hash/MurmurHash3.h"
#include "hash/types.h"
#include "types/key_type.h"

/// @brief  Wrapper around MurmurHash3.
static inline Hash32BitType
Hash32bit(KeyType key)
{
    uint32_t hash = 0;
    MurmurHash3_x86_32(&key, sizeof(key), 0, &hash);
    return hash;
}

/// @brief  Wrapper around MurmurHash3.
static inline Hash64BitType
Hash64bit(KeyType key)
{
    uint64_t hash[2] = {0, 0};
    MurmurHash3_x64_128(&key, sizeof(key), 0, hash);
    return hash[0];
}

/// @brief  Wrapper around MurmurHash3.
static inline struct Hash128BitType
Hash128bit(KeyType key)
{
    struct Hash128BitType hash = {0};
    MurmurHash3_x64_128(&key, sizeof(key), 0, &hash.hash);
    return hash;
}
