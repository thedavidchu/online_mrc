#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <glib.h>

#include "arrays/array_size.h"
#include "histogram/histogram.h"
#include "olken/olken_with_ttl.h"
#include "random/zipfian_random.h"
#include "test/mytester.h"
#include "types/entry_type.h"
#include "unused/mark_unused.h"

const bool PRINT_HISTOGRAM = false;
const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;
const double ZIPFIAN_RANDOM_SKEW = 0.99;

static bool
access_same_key_five_times(void)
{
    EntryType entries[5] = {0, 0, 0, 0, 0};
    uint64_t histogram_oracle_array[11] = {4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    struct Histogram histogram_oracle = {
        .histogram = histogram_oracle_array,
        .num_bins = ARRAY_SIZE(histogram_oracle_array),
        .bin_size = 1,
        .false_infinity = 0,
        .infinity = 1,
        .running_sum = ARRAY_SIZE(entries),
    };

    struct OlkenWithTTL me = {0};
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(OlkenWithTTL__init(&me, histogram_oracle.num_bins, 1));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        OlkenWithTTL__access_item(&me, entries[i], i, SIZE_MAX);
    }
    g_assert_true(
        Histogram__exactly_equal(&me.olken.histogram, &histogram_oracle));
    OlkenWithTTL__destroy(&me);
    return true;
}

/// @brief  Test a deterministic trace against Mattson's histogram.
static bool
small_exact_trace_test(void)
{
    // NOTE These are 100 random integers in the range 0..=10. Generated with
    // Python script:
    // import random; x = [random.randint(0, 10) for _ in range(100)]; print(x)
    EntryType entries[100] = {
        2, 3,  2, 5,  0, 1, 7, 9, 4, 2,  10, 3, 1,  10, 10, 5, 10, 6,  5, 0,
        6, 4,  2, 9,  7, 2, 2, 5, 3, 9,  6,  0, 1,  1,  6,  1, 6,  7,  5, 0,
        0, 10, 8, 3,  1, 2, 6, 7, 3, 10, 8,  6, 10, 6,  6,  2, 6,  0,  7, 9,
        6, 10, 1, 10, 2, 6, 2, 7, 8, 8,  6,  0, 7,  3,  1,  1, 2,  10, 3, 10,
        5, 5,  0, 7,  9, 8, 0, 7, 6, 9,  4,  9, 4,  8,  3,  6, 5,  3,  2, 9};
    uint64_t histogram_oracle_array[11] = {8, 11, 7, 7, 6, 4, 13, 11, 9, 12, 1};
    struct Histogram histogram_oracle = {
        .histogram = histogram_oracle_array,
        .num_bins = ARRAY_SIZE(histogram_oracle_array),
        .bin_size = 1,
        .false_infinity = 0,
        .infinity = 11,
        .running_sum = ARRAY_SIZE(entries),
    };

    struct OlkenWithTTL me = {0};
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(OlkenWithTTL__init(&me, histogram_oracle.num_bins, 1));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        OlkenWithTTL__access_item(&me, entries[i], i, SIZE_MAX);
    }
    g_assert_true(
        Histogram__exactly_equal(&me.olken.histogram, &histogram_oracle));
    OlkenWithTTL__destroy(&me);
    return true;
}

/// @brief  Test a deterministic trace against Mattson's histogram.
///         Specifically, test the false_infinities match.
static bool
small_inexact_trace_test(void)
{
    // NOTE These are 100 random integers in the range 0..=10. Generated with
    // Python script:
    // import random; x = [random.randint(0, 10) for _ in range(100)]; print(x)
    EntryType entries[100] = {
        2, 3,  2, 5,  0, 1, 7, 9, 4, 2,  10, 3, 1,  10, 10, 5, 10, 6,  5, 0,
        6, 4,  2, 9,  7, 2, 2, 5, 3, 9,  6,  0, 1,  1,  6,  1, 6,  7,  5, 0,
        0, 10, 8, 3,  1, 2, 6, 7, 3, 10, 8,  6, 10, 6,  6,  2, 6,  0,  7, 9,
        6, 10, 1, 10, 2, 6, 2, 7, 8, 8,  6,  0, 7,  3,  1,  1, 2,  10, 3, 10,
        5, 5,  0, 7,  9, 8, 0, 7, 6, 9,  4,  9, 4,  8,  3,  6, 5,  3,  2, 9};
    uint64_t histogram_oracle_array[11] = {8, 11, 7, 7, 6, 4, 13, 11, 9, 12, 1};
    struct Histogram histogram_oracle = {
        .histogram = histogram_oracle_array,
        .num_bins = ARRAY_SIZE(histogram_oracle_array) - 2,
        .bin_size = 1,
        .false_infinity =
            histogram_oracle_array[9] + histogram_oracle_array[10],
        .infinity = 11,
        .running_sum = ARRAY_SIZE(entries),
    };

    struct OlkenWithTTL me = {0};
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(OlkenWithTTL__init(&me, histogram_oracle.num_bins, 1));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        OlkenWithTTL__access_item(&me, entries[i], i, SIZE_MAX);
    }
    g_assert_true(
        Histogram__exactly_equal(&me.olken.histogram, &histogram_oracle));
    OlkenWithTTL__destroy(&me);
    return true;
}

static bool
long_trace_test(void)
{
    const uint64_t trace_length = 1 << 20;
    struct ZipfianRandom zrng = {0};
    struct OlkenWithTTL me = {0};

    ASSERT_FUNCTION_RETURNS_TRUE(ZipfianRandom__init(&zrng,
                                                     MAX_NUM_UNIQUE_ENTRIES,
                                                     ZIPFIAN_RANDOM_SKEW,
                                                     0));
    // The maximum trace length is obviously the number of possible unique items
    ASSERT_FUNCTION_RETURNS_TRUE(
        OlkenWithTTL__init(&me, MAX_NUM_UNIQUE_ENTRIES, 1));

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t key = ZipfianRandom__next(&zrng);
        OlkenWithTTL__access_item(&me, key, i, SIZE_MAX);
    }

    if (PRINT_HISTOGRAM) {
        OlkenWithTTL__print_histogram_as_json(&me);
    }

    ZipfianRandom__destroy(&zrng);
    OlkenWithTTL__destroy(&me);
    return true;
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times());
    ASSERT_FUNCTION_RETURNS_TRUE(small_exact_trace_test());
    ASSERT_FUNCTION_RETURNS_TRUE(small_inexact_trace_test());
    ASSERT_FUNCTION_RETURNS_TRUE(long_trace_test());
    return EXIT_SUCCESS;
}
