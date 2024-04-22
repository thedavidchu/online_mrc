#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "arrays/array_size.h"
#include "histogram/basic_histogram.h"
#include "olken/olken.h"
#include "random/zipfian_random.h"
#include "test/mytester.h"
#include "types/entry_type.h"
#include "unused/mark_unused.h"

const bool PRINT_HISTOGRAM = false;
const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;

static bool
access_same_key_five_times(void)
{
    EntryType entries[5] = {0, 0, 0, 0, 0};
    uint64_t histogram_oracle[11] = {4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    struct BasicHistogram basic_histogram_oracle = {
        .histogram = histogram_oracle,
        .length = ARRAY_SIZE(histogram_oracle),
        .false_infinity = 0,
        .infinity = 1,
        .running_sum = ARRAY_SIZE(entries),
    };

    struct OlkenReuseStack me = {0};
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(olken__init(&me, basic_histogram_oracle.length));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        olken__access_item(&me, entries[i]);
    }
    g_assert_true(
        basic_histogram__exactly_equal(&me.histogram, &basic_histogram_oracle));
    olken__destroy(&me);
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
    uint64_t histogram_oracle[11] = {8, 11, 7, 7, 6, 4, 13, 11, 9, 12, 1};
    struct BasicHistogram basic_histogram_oracle = {
        .histogram = histogram_oracle,
        .length = ARRAY_SIZE(histogram_oracle),
        .false_infinity = 0,
        .infinity = 11,
        .running_sum = ARRAY_SIZE(entries),
    };

    struct OlkenReuseStack me = {0};
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(olken__init(&me, basic_histogram_oracle.length));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        olken__access_item(&me, entries[i]);
    }
    g_assert_true(
        basic_histogram__exactly_equal(&me.histogram, &basic_histogram_oracle));
    olken__destroy(&me);
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
    uint64_t histogram_oracle[11] = {8, 11, 7, 7, 6, 4, 13, 11, 9, 12, 1};
    struct BasicHistogram basic_histogram_oracle = {
        .histogram = histogram_oracle,
        .length = ARRAY_SIZE(histogram_oracle) - 2,
        .false_infinity = histogram_oracle[9] + histogram_oracle[10],
        .infinity = 11,
        .running_sum = ARRAY_SIZE(entries),
    };

    struct OlkenReuseStack me = {0};
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(olken__init(&me, basic_histogram_oracle.length));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        olken__access_item(&me, entries[i]);
    }
    g_assert_true(
        basic_histogram__exactly_equal(&me.histogram, &basic_histogram_oracle));
    olken__destroy(&me);
    return true;
}

static bool
long_trace_test(void)
{
    const uint64_t trace_length = 1 << 20;
    struct ZipfianRandom zrng = {0};
    struct OlkenReuseStack me = {0};

    ASSERT_FUNCTION_RETURNS_TRUE(
        zipfian_random__init(&zrng, MAX_NUM_UNIQUE_ENTRIES, 0.5, 0));
    // The maximum trace length is obviously the number of possible unique items
    ASSERT_FUNCTION_RETURNS_TRUE(olken__init(&me, MAX_NUM_UNIQUE_ENTRIES));

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t key = zipfian_random__next(&zrng);
        olken__access_item(&me, key);
    }

    if (PRINT_HISTOGRAM) {
        olken__print_histogram_as_json(&me);
    }

    olken__destroy(&me);
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
