/** @brief  Test the performance and distribution of various hash functions.
 *
 *  @note   I am considering separating the performance and distribution
 *          tests into two separate files. The only problem is the code
 *          shared between the two will need to be placed in a header,
 *          thereby adding a lot of complexity.
 *  @note   I am also considering flattening the 'bench' directory so
 *          that it doesn't have directories. This is simply because I
 *          do not have a lot of benchmarks currently. My trepidation is
 *          that I'd need to change it back if I do add more benchmarks.
 */
#include <stdint.h>
#include <stdlib.h>

#include "arrays/array_size.h"
#include "hash/MurmurHash3.h"
#include "hash/miscellaneous_hash.h"
#include "hash/splitmix64.h"
#include "logger/logger.h"
#include "timer/timer.h"
#include "unused/mark_unused.h"

#define TIME_HASH(f)         time_hash(f, #f)
#define TEST_DISTRIBUTION(f) test_hash_distribution(f, #f)

size_t const NUM_VALUES_FOR_PERF = 1 << 28;
size_t const NUM_VALUES_FOR_DISTRIBUTION = 1 << 20;

////////////////////////////////////////////////////////////////////////////////
/// HASH FUNCTION WRAPPERS FOR UNIFIED INTERFACE
////////////////////////////////////////////////////////////////////////////////

static inline uint64_t
wrap_MurmurHash3_x64_128(uint64_t const key)
{
    uint64_t hash[2] = {0, 0};
    MurmurHash3_x64_128(&key, sizeof(key), 0, hash);
    return hash[0];
}

static inline uint64_t
wrap_RSHash(uint64_t const key)
{
    return RSHash((void const *)&key, sizeof(key));
}

static inline uint64_t
wrap_SDBMHash(uint64_t const key)
{
    return SDBMHash((void const *)&key, sizeof(key));
}

static inline uint64_t
wrap_APHash(uint64_t const key)
{
    return APHash((void const *)&key, sizeof(key));
}

////////////////////////////////////////////////////////////////////////////////
/// TESTING HELPER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

/// @brief  Time how long it takes to run a certain number of hashes.
/// @note   I try to inline this function so that the function pointers
///         are inlined too.
static inline void
time_hash(uint64_t (*const f)(uint64_t const key), char const *const fname)
{
    double const t0 = get_wall_time_sec();
    for (size_t i = 0; i < NUM_VALUES_FOR_PERF; ++i) {
        volatile uint64_t x = f(i);
        UNUSED(x);
    }
    double const t1 = get_wall_time_sec();
    LOGGER_INFO("%s time: %f", fname, t1 - t0);
}

/// @note   Taken from the cppreference website.
///         Source: https://en.cppreference.com/w/c/algorithm/qsort
int
compare_ints(const void *a, const void *b)
{
    int arg1 = *(const int *)a;
    int arg2 = *(const int *)b;

    if (arg1 < arg2) {
        return -1;
    }
    if (arg1 > arg2) {
        return 1;
    }
    return 0;

    // return (arg1 > arg2) - (arg1 < arg2); // possible shortcut

    // return arg1 - arg2; // erroneous shortcut: undefined behavior in case of
    // integer overflow, such as with INT_MIN here
}

int
get_median(int const *const counts, size_t const length)
{
    if (length == 0) {
        return 0;
    }
    if (length == 1) {
        return counts[0];
    }
    if (length % 2 == 0) {
        return (counts[length / 2] + counts[length / 2 + 1]) / 2;
    }
    // NOTE This is correct. Take this as an example:
    //      counts = [ 0 1 2 3 4 ], length = 5
    //      => length / 2 = floor(div(length, 2)) = 2. QED.
    return counts[length / 2];
}

/// @brief  Test the distribution of hash functions in a hash table.
static inline void
test_hash_distribution(uint64_t (*const f)(uint64_t const key),
                       char const *const fname)
{
    // NOTE I use the 'int' type here because there shouldn't be that
    //      many collisions..., right? Less memory means it runs faster.
    //      I still feel a little bit sketched out by this. I love using
    //      large sizes for my integers.
    int counts[100] = {0};
    double const t0 = get_wall_time_sec();
    for (size_t i = 0; i < NUM_VALUES_FOR_DISTRIBUTION; ++i) {
        ++counts[f(i) % ARRAY_SIZE(counts)];
    }
    qsort(counts, ARRAY_SIZE(counts), sizeof(*counts), compare_ints);
    int const min_collisions = counts[0];
    int const max_collisions = counts[ARRAY_SIZE(counts) - 1];
    int const median_collisions = get_median(counts, ARRAY_SIZE(counts));
    double const t1 = get_wall_time_sec();
    LOGGER_INFO("%s time: %f | max collisions: %d | median collisions: %d | "
                "min collisions: %d",
                fname,
                t1 - t0,
                max_collisions,
                median_collisions,
                min_collisions);
}

int
main(void)
{
    TIME_HASH(wrap_MurmurHash3_x64_128);
    TIME_HASH(splitmix64_hash);
    TIME_HASH(wrap_RSHash);
    TIME_HASH(wrap_SDBMHash);
    TIME_HASH(wrap_APHash);

    TEST_DISTRIBUTION(wrap_MurmurHash3_x64_128);
    TEST_DISTRIBUTION(splitmix64_hash);
    TEST_DISTRIBUTION(wrap_RSHash);
    TEST_DISTRIBUTION(wrap_SDBMHash);
    TEST_DISTRIBUTION(wrap_APHash);
    return 0;
}
