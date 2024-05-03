#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "arrays/array_size.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "parda.h"
#include "parda_shards/parda_fixed_rate_shards.h"
#include "random/zipfian_random.h"
#include "shards/fixed_rate_shards.h"
#include "test/mytester.h"
#include "unused/mark_unused.h"

const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;
const uint64_t trace_length = 1 << 20;
const double ZIPFIAN_RANDOM_SKEW = 0.99;

static bool
access_same_key_five_times(void)
{
    EntryType entries[5] = {0, 0, 0, 0, 0};
    struct Olken oracle = {0};
    struct FixedRateShards me = {0};

    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(Olken__init(&oracle, MAX_NUM_UNIQUE_ENTRIES));
    g_assert_true(FixedRateShards__init(&me, MAX_NUM_UNIQUE_ENTRIES, 1));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        uint64_t entry = entries[i];
        Olken__access_item(&oracle, entry);
        FixedRateShards__access_item(&me, entry);
    }
    struct MissRateCurve oracle_mrc = {0}, mrc = {0};
    MissRateCurve__init_from_histogram(&oracle_mrc, &oracle.histogram);
    MissRateCurve__init_from_histogram(&mrc, &me.olken.histogram);
    double mse = MissRateCurve__mean_squared_error(&oracle_mrc, &mrc);
    LOGGER_INFO("Mean-Squared Error: %lf", mse);
    g_assert_cmpfloat(mse, <=, 0.000001);

    Olken__destroy(&oracle);
    FixedRateShards__destroy(&me);
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
    struct Olken oracle = {0};
    struct FixedRateShards me = {0};

    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(Olken__init(&oracle, MAX_NUM_UNIQUE_ENTRIES));
    g_assert_true(FixedRateShards__init(&me, MAX_NUM_UNIQUE_ENTRIES, 1));

    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        uint64_t entry = entries[i];
        Olken__access_item(&oracle, entry);
        FixedRateShards__access_item(&me, entry);
    }
    struct MissRateCurve oracle_mrc = {0}, mrc = {0};
    MissRateCurve__init_from_histogram(&oracle_mrc, &oracle.histogram);
    MissRateCurve__init_from_histogram(&mrc, &me.olken.histogram);
    double mse = MissRateCurve__mean_squared_error(&oracle_mrc, &mrc);
    LOGGER_INFO("Mean-Squared Error: %lf", mse);
    g_assert_cmpfloat(mse, <=, 0.000001);

    Olken__destroy(&oracle);
    FixedRateShards__destroy(&me);
    return true;
}

static bool
long_accuracy_trace_test(void)
{
    struct ZipfianRandom zrng = {0};
    struct Olken oracle = {0};
    struct FixedRateShards me = {0};

    g_assert_true(ZipfianRandom__init(&zrng,
                                      MAX_NUM_UNIQUE_ENTRIES,
                                      ZIPFIAN_RANDOM_SKEW,
                                      0));
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(Olken__init(&oracle, MAX_NUM_UNIQUE_ENTRIES));
    g_assert_true(FixedRateShards__init(&me, MAX_NUM_UNIQUE_ENTRIES, 1e-3));

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t entry = ZipfianRandom__next(&zrng);
        Olken__access_item(&oracle, entry);
        FixedRateShards__access_item(&me, entry);
    }
    struct MissRateCurve oracle_mrc = {0}, mrc = {0};
    MissRateCurve__init_from_histogram(&oracle_mrc, &oracle.histogram);
    MissRateCurve__init_from_histogram(&mrc, &me.olken.histogram);
    double mse = MissRateCurve__mean_squared_error(&oracle_mrc, &mrc);
    LOGGER_INFO("Mean-Squared Error: %lf", mse);
    g_assert_cmpfloat(mse, <=, 0.04);

    ZipfianRandom__destroy(&zrng);
    Olken__destroy(&oracle);
    FixedRateShards__destroy(&me);
    return true;
}

static bool
long_parda_matching_trace_test(void)
{
    struct ZipfianRandom zrng = {0};
    struct PardaFixedRateShards oracle = {0};
    struct FixedRateShards me = {0};

    g_assert_true(ZipfianRandom__init(&zrng,
                                      MAX_NUM_UNIQUE_ENTRIES,
                                      ZIPFIAN_RANDOM_SKEW,
                                      0));
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(PardaFixedRateShards__init(&oracle, 1e-3));
    g_assert_true(FixedRateShards__init(&me, MAX_NUM_UNIQUE_ENTRIES, 1e-3));

    // NOTE We (theoretically) need to use a trace that cannot produce
    //      more items than PARDA or my implementation can handle with
    //      100% accuracy. In practice, PARDA can handle fewer (and it
    //      is not a configurable limit, unfortunately). In practice,
    //      due to the random skew, it doesn't really make a difference.
    size_t my_trace_length = MIN((size_t)nbuckets, MAX_NUM_UNIQUE_ENTRIES);
    for (uint64_t i = 0; i < my_trace_length; ++i) {
        uint64_t entry = ZipfianRandom__next(&zrng);
        PardaFixedRateShards__access_item(&oracle, entry);
        FixedRateShards__access_item(&me, entry);
    }
    struct MissRateCurve oracle_mrc = {0}, mrc = {0};
    MissRateCurve__init_from_parda_histogram(
        &oracle_mrc,
        nbuckets,
        oracle.program_data.histogram,
        oracle.current_time_stamp,
        oracle.program_data.histogram[B_OVFL]);
    MissRateCurve__init_from_histogram(&mrc, &me.olken.histogram);
    double mse = MissRateCurve__mean_squared_error(&oracle_mrc, &mrc);
    LOGGER_INFO("Mean-Squared Error: %lf\n", mse);
    g_assert_true(mse == 0.000000);

    ZipfianRandom__destroy(&zrng);
    PardaFixedRateShards__destroy(&oracle);
    FixedRateShards__destroy(&me);
    return true;
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times());
    ASSERT_FUNCTION_RETURNS_TRUE(small_exact_trace_test());
    ASSERT_FUNCTION_RETURNS_TRUE(long_accuracy_trace_test());
    ASSERT_FUNCTION_RETURNS_TRUE(long_parda_matching_trace_test());
    return EXIT_SUCCESS;
}
