#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "arrays/array_size.h"
#include "histogram/basic_histogram.h"
#include "quickmrc/quickmrc.h"
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
    // We round up the stack distance with QuickMRC
    uint64_t histogram_oracle[11] = {0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    struct BasicHistogram basic_histogram_oracle = {
        .histogram = histogram_oracle,
        .length = ARRAY_SIZE(histogram_oracle),
        .false_infinity = 0,
        .infinity = 1,
        .running_sum = ARRAY_SIZE(entries),
    };

    struct QuickMrc me = {0};
    // The maximum trace length is obviously the number of possible unique items
    ASSERT_TRUE_WITHOUT_CLEANUP(
        quickmrc__init(&me, 60, 100, basic_histogram_oracle.length));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        ASSERT_TRUE_WITHOUT_CLEANUP(quickmrc__access_item(&me, entries[i]));
    }
    ASSERT_TRUE_OR_CLEANUP(
        basic_histogram__exactly_equal(&me.histogram, &basic_histogram_oracle),
        quickmrc__destroy(&me));
    quickmrc__destroy(&me);
    return true;
}

/// @brief  Test a deterministic trace against Mattson's histogram.
static bool
small_merge_test(void)
{
    // NOTE These are 100 random integers in the range 0..=10. Generated with
    // Python script:
    // import random; x = [random.randint(0, 10) for _ in range(100)]; print(x)
    uint64_t histogram_oracle[11] = {0};
    struct BasicHistogram basic_histogram_oracle = {
        .histogram = histogram_oracle,
        .length = ARRAY_SIZE(histogram_oracle),
        .false_infinity = 0,
        .infinity = 1000,
        .running_sum = 1000,
    };

    struct QuickMrc me = {0};
    // The maximum trace length is obviously the number of possible unique items
    ASSERT_TRUE_WITHOUT_CLEANUP(
        quickmrc__init(&me, 60, 100, basic_histogram_oracle.length));
    for (uint64_t i = 0; i < 1000; ++i) {
        quickmrc__access_item(&me, i);
    }

    if (PRINT_HISTOGRAM) {
        quickmrc__print_histogram_as_json(&me);
    }
    ASSERT_TRUE_OR_CLEANUP(
        basic_histogram__exactly_equal(&me.histogram, &basic_histogram_oracle),
        quickmrc__destroy(&me));
    quickmrc__destroy(&me);
    return true;
}

static bool
long_trace_test(void)
{
    const uint64_t trace_length = 1 << 20;
    struct ZipfianRandom zrng = {0};
    struct QuickMrc me = {0};

    ASSERT_FUNCTION_RETURNS_TRUE(
        zipfian_random__init(&zrng, MAX_NUM_UNIQUE_ENTRIES, 0.5, 0));
    // The maximum trace length is obviously the number of possible unique items
    ASSERT_FUNCTION_RETURNS_TRUE(
        quickmrc__init(&me, 60, 100, MAX_NUM_UNIQUE_ENTRIES));

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t key = zipfian_random__next(&zrng);
        quickmrc__access_item(&me, key);
    }

    if (PRINT_HISTOGRAM) {
        quickmrc__print_histogram_as_json(&me);
    }

    quickmrc__destroy(&me);
    return true;
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times());
    ASSERT_FUNCTION_RETURNS_TRUE(small_merge_test());
    // ASSERT_FUNCTION_RETURNS_TRUE(small_inexact_trace_test());
    ASSERT_FUNCTION_RETURNS_TRUE(long_trace_test());
    return EXIT_SUCCESS;
}
