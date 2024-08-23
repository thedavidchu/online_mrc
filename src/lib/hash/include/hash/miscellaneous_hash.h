/** @brief   Miscellaneous string hash functions.
 *
 *  @note   I changed the 'unsigned int' to 'uint64_t' and 'size_t',
 *          depending on the functionality.
 *  @note   I mark all these functions as 'static inline' and place them
 *          in this header so the compiler can optionally inline them
 *          easily without needing to do link-time optimizations. As you
 *          know, I have a love-hate relationship with link-time
 *          optimizations.
 *
 *  Source: https://www.partow.net/programming/hashfunctions/
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/// @note   A simple hash function from Robert Sedgwicks Algorithms in C book.
///         I've added some simple optimizations to the algorithm in order to
///         speed up its hashing process.
static inline uint64_t
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

/// @note   A bitwise hash function written by Justin Sobel.
static inline uint64_t
JSHash(char const *str, size_t const length)
{
    uint64_t hash = 1315423911;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash ^= ((hash << 5) + (*str) + (hash >> 2));
    }

    return hash;
}

/// @note   This hash algorithm is based on work by Peter J. Weinberger of
///         Renaissance Technologies. The book Compilers (Principles, Techniques
///         and Tools) by Aho, Sethi and Ulman, recommends the use of hash
///         functions that employ the hashing methodology found in this
///         particular algorithm.
static inline uint64_t
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

/// @note   Similar to the PJW Hash function, but tweaked for 32-bit processors.
///         It is a widley used hash function on UNIX based systems.
static inline uint64_t
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

/// @note   This hash function comes from Brian Kernighan and Dennis Ritchie's
///         book "The C Programming Language". It is a simple hash function
///         using a strange set of possible seeds which all constitute a
///         pattern of 31....31...31 etc, it seems to be very similar to the
///         DJB hash function.
static inline uint64_t
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

/// @note   This is the algorithm of choice which is used in the open
///         source SDBM project. The hash function seems to have a good over-all
///         distribution for many different data sets. It seems to work well
///         in situations where there is a high variance in the MSBs of the
///         elements in a data set.
static inline uint64_t
SDBMHash(char const *str, size_t const length)
{
    uint64_t hash = 0;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash = (*str) + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

/// @note   An algorithm produced by Professor Daniel J. Bernstein and shown
///         first to the world on the usenet newsgroup comp.lang.c. It is
///         one of the most efficient hash functions ever published.
static inline uint64_t
DJBHash(char const *str, size_t const length)
{
    uint64_t hash = 5381;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash = ((hash << 5) + hash) + (*str);
    }

    return hash;
}

/// @note   An algorithm proposed by Donald E. Knuth in The Art Of Computer
///         Programming Volume 3, under the topic of sorting and search
///         chapter 6.4.
static inline uint64_t
DEKHash(char const *str, size_t const length)
{
    uint64_t hash = length;
    size_t i = 0;

    for (i = 0; i < length; ++str, ++i) {
        hash = ((hash << 5) ^ (hash >> 27)) ^ (*str);
    }

    return hash;
}

/// @note   An algorithm produced by me Arash Partow. I took ideas from all
///         of the above hash functions making a hybrid rotative and
///         additive hash function algorithm. There isn't any real
///         mathematical analysis explaining why one should use this hash
///         function instead of the others described above other than the
///         fact that I tired to resemble the design as close as possible to
///         a simple LFSR. An empirical result which demonstrated the
///         distributive abilities of the hash algorithm was obtained using
///         a hash-table with 100003 buckets, hashing The Project Gutenberg
///         Etext of Webster's Unabridged Dictionary, the longest
///         encountered chain length was 7, the average chain length was 2,
///         the number of empty buckets was 4579.
static inline uint64_t
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
