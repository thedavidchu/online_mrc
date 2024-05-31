#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <pthread.h>

#include "arrays/array_size.h"
#include "histogram/histogram.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "quickmrc/bucketed_quickmrc.h"
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
    uint64_t histogram_oracle_array[11] = {0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    struct Histogram histogram_oracle = {
        .histogram = histogram_oracle_array,
        .num_bins = ARRAY_SIZE(histogram_oracle_array),
        .bin_size = 1,
        .false_infinity = 0,
        .infinity = 1,
        .running_sum = ARRAY_SIZE(entries),
    };

    struct BucketedQuickMRC me = {0};
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(BucketedQuickMRC__init(&me,
                                         60,
                                         100,
                                         histogram_oracle.num_bins,
                                         1.0,
                                         1 << 13));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        g_assert_true(BucketedQuickMRC__access_item(&me, entries[i]));
    }
    g_assert_true(Histogram__exactly_equal(&me.histogram, &histogram_oracle));
    BucketedQuickMRC__destroy(&me);
    return true;
}

/// @brief  Test a deterministic trace against Mattson's histogram.
static bool
small_merge_test(void)
{
    // NOTE These are 100 random integers in the range 0..=10. Generated with
    // Python script:
    // import random; x = [random.randint(0, 10) for _ in range(100)]; print(x)
    uint64_t histogram_oracle_array[11] = {0};
    struct Histogram histogram_oracle = {
        .histogram = histogram_oracle_array,
        .num_bins = ARRAY_SIZE(histogram_oracle_array),
        .bin_size = 1,
        .false_infinity = 0,
        .infinity = 1000,
        .running_sum = 1000,
    };

    struct BucketedQuickMRC me = {0};
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(BucketedQuickMRC__init(&me,
                                         60,
                                         100,
                                         histogram_oracle.num_bins,
                                         1.0,
                                         1 << 13));
    for (uint64_t i = 0; i < 1000; ++i) {
        BucketedQuickMRC__access_item(&me, i);
    }

    if (PRINT_HISTOGRAM) {
        BucketedQuickMRC__print_histogram_as_json(&me);
    }
    g_assert_true(Histogram__exactly_equal(&me.histogram, &histogram_oracle));
    BucketedQuickMRC__destroy(&me);
    return true;
}

static bool
long_trace_test(void)
{
    const uint64_t trace_length = 1 << 20;
    struct ZipfianRandom zrng = {0};
    struct BucketedQuickMRC me = {0};

    ASSERT_FUNCTION_RETURNS_TRUE(
        ZipfianRandom__init(&zrng, MAX_NUM_UNIQUE_ENTRIES, 0.5, 0));
    // The maximum trace length is obviously the number of possible unique items
    ASSERT_FUNCTION_RETURNS_TRUE(BucketedQuickMRC__init(&me,
                                                        60,
                                                        100,
                                                        MAX_NUM_UNIQUE_ENTRIES,
                                                        1.0,
                                                        1 << 13));

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t key = ZipfianRandom__next(&zrng);
        BucketedQuickMRC__access_item(&me, key);
    }

    if (PRINT_HISTOGRAM) {
        BucketedQuickMRC__print_histogram_as_json(&me);
    }

    ZipfianRandom__destroy(&zrng);
    BucketedQuickMRC__destroy(&me);
    return true;
}

static bool
mean_absolute_error_test(void)
{
    const uint64_t trace_length = 1 << 20;
    struct ZipfianRandom zrng = {0};
    struct BucketedQuickMRC me = {0};
    struct Olken olken = {0};

    g_assert_true(ZipfianRandom__init(&zrng, MAX_NUM_UNIQUE_ENTRIES, 0.5, 0));
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(BucketedQuickMRC__init(&me,
                                         60,
                                         100,
                                         MAX_NUM_UNIQUE_ENTRIES,
                                         1.0,
                                         1 << 13));
    g_assert_true(Olken__init(&olken, MAX_NUM_UNIQUE_ENTRIES, 1));

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t key = ZipfianRandom__next(&zrng);
        BucketedQuickMRC__access_item(&me, key);
        Olken__access_item(&olken, key);
    }

    struct MissRateCurve my_mrc = {0}, olken_mrc = {0};
    g_assert_true(MissRateCurve__init_from_histogram(&my_mrc, &me.histogram));
    g_assert_true(
        MissRateCurve__init_from_histogram(&olken_mrc, &olken.histogram));
    double mae = MissRateCurve__mean_absolute_error(&my_mrc, &olken_mrc);
    printf("Mean Absolute Error: %f\n", mae);

    ZipfianRandom__destroy(&zrng);
    BucketedQuickMRC__destroy(&me);
    Olken__destroy(&olken);
    return true;
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times());
#if 0
    ASSERT_FUNCTION_RETURNS_TRUE(small_merge_test());
#endif
    UNUSED(small_merge_test);
    ASSERT_FUNCTION_RETURNS_TRUE(mean_absolute_error_test());
    ASSERT_FUNCTION_RETURNS_TRUE(long_trace_test());
    return EXIT_SUCCESS;
}
