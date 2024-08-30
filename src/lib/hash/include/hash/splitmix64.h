#pragma once

#include <stdint.h>

#include "hash/types.h"

/// @brief  This is a 64-bit hash function based on the MurmurHash3 64-bit hash.
///
/// Link to Stack Overflow source:
/// https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
///
/// Link to Stack Overflow's original source:
/// https://xorshift.di.unimi.it/splitmix64.c
///
/// Link to blog post about finding these magic numbers:
/// https://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html
static inline Hash64BitType
splitmix64_hash(const uint64_t key)
{
    // NOTE The original source (xorshift.di.unimi.it) adds this
    //      constant to the key, so that is why I am now adding it.
    //      Otherwise, hashing 0 returns 0, which is a pain for my
    //      Zipfian distribution.
    uint64_t k = key + 0x9e3779b97f4a7c15ULL;
    // NOTE I use the suffix '*ULL' to denote that the literal is at
    //      least an int64. I've had weird bugs in the past to do with
    //      literal conversion. I'm not sure the details. I only
    //      remember it was a huge pain.
    k = ((k >> 30) ^ k) * 0xbf58476d1ce4e5b9ULL;
    k = ((k >> 27) ^ k) * 0x94d049bb133111ebULL;
    k = (k >> 31) ^ k;
    return k;
}

/// @brief  Reverse the splitmix64_hash function.
/// @note   Source:
///         https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
static inline uint64_t
reverse_splitmix64_hash(Hash64BitType const hash)
{
    uint64_t h = hash;
    h = (h ^ (h >> 31) ^ (h >> 62)) * UINT64_C(0x319642b2d24d8ec3);
    h = (h ^ (h >> 27) ^ (h >> 54)) * UINT64_C(0x96de1b173f119089);
    h = h ^ (h >> 30) ^ (h >> 60);
    return h - UINT64_C(0x9e3779b97f4a7c15);
}
