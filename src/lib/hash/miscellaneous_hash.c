#include <stddef.h>
#include <stdint.h>

uint64_t
RSHash(char const *str, size_t const length)
{
    uint64_t b = 378551;
    uint64_t a = 63689;
    uint64_t hash = 0;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash = hash * a + (*str);
        a = a * b;
    }

    return hash;
}

uint64_t
JSHash(char const *str, size_t const length)
{
    uint64_t hash = 1315423911;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash ^= ((hash << 5) + (*str) + (hash >> 2));
    }

    return hash;
}

uint64_t
PJWHash(char const *str, size_t const length)
{
    const uint64_t BitsInUnsignedInt = (uint64_t)(sizeof(uint64_t) * 8);
    const uint64_t ThreeQuarters = (uint64_t)((BitsInUnsignedInt * 3) / 4);
    const uint64_t OneEighth = (uint64_t)(BitsInUnsignedInt / 8);
    const uint64_t HighBits = (uint64_t)(0xFFFFFFFF)
                              << (BitsInUnsignedInt - OneEighth);
    uint64_t hash = 0;
    uint64_t test = 0;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash = (hash << OneEighth) + (*str);

        if ((test = hash & HighBits) != 0) {
            hash = ((hash ^ (test >> ThreeQuarters)) & (~HighBits));
        }
    }

    return hash;
}

uint64_t
ELFHash(char const *str, size_t const length)
{
    uint64_t hash = 0;
    uint64_t x = 0;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash = (hash << 4) + (*str);

        if ((x = hash & 0xF0000000L) != 0) {
            hash ^= (x >> 24);
        }

        hash &= ~x;
    }

    return hash;
}

uint64_t
BKDRHash(char const *str, size_t const length)
{
    uint64_t seed = 131; /* 31 131 1313 13131 131313 etc.. */
    uint64_t hash = 0;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash = (hash * seed) + (*str);
    }

    return hash;
}

uint64_t
SDBMHash(char const *str, size_t const length)
{
    uint64_t hash = 0;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash = (*str) + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

uint64_t
DJBHash(char const *str, size_t const length)
{
    uint64_t hash = 5381;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash = ((hash << 5) + hash) + (*str);
    }

    return hash;
}

uint64_t
DEKHash(char const *str, size_t const length)
{
    uint64_t hash = length;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash = ((hash << 5) ^ (hash >> 27)) ^ (*str);
    }

    return hash;
}

uint64_t
APHash(char const *str, size_t const length)
{
    uint64_t hash = 0xAAAAAAAA;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash ^= ((i & 1) == 0) ? ((hash << 7) ^ (*str) * (hash >> 3))
                               : (~((hash << 11) + ((*str) ^ (hash >> 5))));
    }

    return hash;
}
