/** @brief  Wrappers for hash functions. */
#pragma once

#include <stdint.h>

#include "hash/MurmurHash3.h"
#include "hash/miscellaneous_hash.h"
#include "hash/splitmix64.h"
#include "hash/types.h"
#include "types/key_type.h"

#ifndef HASH_FUNCTION_SELECT
#define HASH_FUNCTION_SELECT 2
#endif

static inline Hash32BitType
Hash32Bit(KeyType const key)
{
    uint32_t hash = 0;
    MurmurHash3_x86_32(&key, sizeof(key), 0, &hash);
    return hash;
}

static inline Hash64BitType
Hash64Bit(KeyType const key)
{
    switch (HASH_FUNCTION_SELECT) {
    case 0: {
        uint64_t hash[2] = {0, 0};
        MurmurHash3_x64_128(&key, sizeof(key), 0, hash);
        return hash[0];
    }
    case 1:
        return splitmix64_hash(key);
    case 2:
        return RSHash((void const *)&key, sizeof(key));
    case 3:
        return SDBMHash((void const *)&key, sizeof(key));
    case 4:
        return APHash((void const *)&key, sizeof(key));
    default:
#if HASH_FUNCTION_SELECT < 0 || HASH_FUNCTION_SELECT > 4
// NOTE This could go anywhere, but I decided to stick it with the code
//      that would be generated in this case.
#error "invalid value for HASH_FUNCTION_SELECT selected!"
#endif
        return key;
    }
}

static inline struct Hash128BitType
Hash128Bit(KeyType const key)
{
    struct Hash128BitType hash = {0};
    MurmurHash3_x64_128(&key, sizeof(key), 0, &hash.hash);
    return hash;
}
