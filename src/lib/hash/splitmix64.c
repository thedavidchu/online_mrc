#include <stdint.h>

#include "hash/splitmix64.h"

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
Hash64BitType
splitmix64_hash(const uint64_t key)
{
    uint64_t k = key;
    // I use the suffix '*ULL' to denote that the literal is at least an int64.
    // I've had weird bugs in the past to do with literal conversion. I'm not sure
    // the details. I only remember it was a huge pain.
    k = ((k >> 30) ^ k) * 0xbf58476d1ce4e5b9ULL;
    k = ((k >> 27) ^ k) * 0x94d049bb133111ebULL;
    k = (k >> 31) ^ k;
    return k;
}
