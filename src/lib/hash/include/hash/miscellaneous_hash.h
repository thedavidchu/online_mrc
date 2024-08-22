/** @brief   Miscellaneous string hash functions.
 *
 *  @note   I changed the 'unsigned int' to 'uint64_t' and 'size_t',
 *          depending on the functionality.
 *
 *  Source: https://www.partow.net/programming/hashfunctions/#RSHashFunction
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/// @note   A simple hash function from Robert Sedgwicks Algorithms in C book.
///         I've added some simple optimizations to the algorithm in order to
///         speed up its hashing process.
uint64_t
RSHash(char const *str, size_t const length);

/// @note   A bitwise hash function written by Justin Sobel.
uint64_t
JSHash(char const *str, size_t const length);

/// @note   This hash algorithm is based on work by Peter J. Weinberger of
///         Renaissance Technologies. The book Compilers (Principles, Techniques
///         and Tools) by Aho, Sethi and Ulman, recommends the use of hash
///         functions that employ the hashing methodology found in this
///         particular algorithm.
uint64_t
PJWHash(char const *str, size_t const length);

/// @note   Similar to the PJW Hash function, but tweaked for 32-bit processors.
///         It is a widley used hash function on UNIX based systems.
uint64_t
ELFHash(char const *str, size_t const length);

/// @note   This hash function comes from Brian Kernighan and Dennis Ritchie's
///         book "The C Programming Language". It is a simple hash function
///         using a strange set of possible seeds which all constitute a pattern
///         of 31....31...31 etc, it seems to be very similar to the DJB hash
///         function.
uint64_t
BKDRHash(char const *str, size_t const length);

/// @note   This is the algorithm of choice which is used in the open source
///         SDBM project. The hash function seems to have a good over-all
///         distribution for many different data sets. It seems to work well in
///         situations where there is a high variance in the MSBs of the
///         elements in a data set.
uint64_t
SDBMHash(char const *str, size_t const length);

/// @note   An algorithm produced by Professor Daniel J. Bernstein and shown
///         first to the world on the usenet newsgroup comp.lang.c. It is one of
///         the most efficient hash functions ever published.
uint64_t
DJBHash(char const *str, size_t const length);

/// @note   An algorithm proposed by Donald E. Knuth in The Art Of Computer
///         Programming Volume 3, under the topic of sorting and search
///         chapter 6.4.
uint64_t
DEKHash(char const *str, size_t const length);

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
uint64_t
APHash(char const *str, size_t const length);
