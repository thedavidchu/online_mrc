#pragma once

#include <stdint.h>

#include "hash/MurmurHash3.h"
#include "hash/types.h"
#include "types/key_type.h"

/// @brief  Wrapper around MurmurHash3.
static inline Hash64BitType
MyMurmurHash3(KeyType key)
{
    uint64_t hash[2] = {0, 0};
    MurmurHash3_x64_128(&key, sizeof(key), 0, hash);
    return hash[0];
}
