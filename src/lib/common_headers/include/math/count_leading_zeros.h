#pragma once

#include <stdint.h>

/// @brief  Count leading zeros in a uint64_t.
/// @note   The value of __builtin_clzll(0) is undefined, as per GCC's docs.
/// @note   This function is general-purpose enough that I don't feel
///         the need to prefix it with 'EvictingHashTable__' or 'EHT__'.
static inline int
clz(uint64_t x)
{
    return x == 0 ? 8 * sizeof(x) : __builtin_clzll(x);
}
