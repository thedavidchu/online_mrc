#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "arrays/array_size.h"
#include "evicting_map/evicting_map.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "random/zipfian_random.h"
#include "test/mytester.h"
#include "unused/mark_unused.h"

const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;
const uint64_t TRACE_LENGTH = 1 << 20;
const double ZIPFIAN_RANDOM_SKEW = 0.99;

static bool
access_same_key_five_times(void)
{
    EntryType entries[5] = {0, 0, 0, 0, 0};
    struct Olken oracle = {0};
    struct EvictingMap me = {0};

    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(Olken__init(&oracle, MAX_NUM_UNIQUE_ENTRIES, 1));
    g_assert_true(EvictingMap__init(&me, 1.0, MAX_NUM_UNIQUE_ENTRIES, 1, 1));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        uint64_t entry = entries[i];
        Olken__access_item(&oracle, entry);
        EvictingMap__access_item(&me, entry);
    }
    struct MissRateCurve oracle_mrc = {0}, mrc = {0};
    MissRateCurve__init_from_histogram(&oracle_mrc, &oracle.histogram);
    MissRateCurve__init_from_histogram(&mrc, &me.histogram);
    double mse = MissRateCurve__mean_squared_error(&oracle_mrc, &mrc);
    LOGGER_INFO("Mean-Squared Error: %lf", mse);
    g_assert_cmpfloat(mse, <=, 0.07);

    Olken__destroy(&oracle);
    EvictingMap__destroy(&me);
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
    struct EvictingMap me = {0};

    // The maximum trace length is obviously the number of possible unique items
    // I deliberately underestimate it (it should be 11).
    g_assert_true(Olken__init(&oracle, 10, 1));
    g_assert_true(EvictingMap__init(&me, 1.0, 1024, 10, 1));

    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        uint64_t entry = entries[i];
        Olken__access_item(&oracle, entry);
        EvictingMap__access_item(&me, entry);
    }
    struct MissRateCurve oracle_mrc = {0}, mrc = {0};
    MissRateCurve__init_from_histogram(&oracle_mrc, &oracle.histogram);
    MissRateCurve__init_from_histogram(&mrc, &me.histogram);
    double mse = MissRateCurve__mean_squared_error(&oracle_mrc, &mrc);
    LOGGER_INFO("Mean-Squared Error: %lf", mse);
    g_assert_cmpfloat(mse, <=, 0.17);

    Olken__destroy(&oracle);
    EvictingMap__destroy(&me);
    return true;
}

static bool
long_accuracy_trace_test(void)
{
    struct ZipfianRandom zrng = {0};
    struct Olken oracle = {0};
    struct EvictingMap me = {0};

    g_assert_true(ZipfianRandom__init(&zrng,
                                      MAX_NUM_UNIQUE_ENTRIES,
                                      ZIPFIAN_RANDOM_SKEW,
                                      0));
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(Olken__init(&oracle, MAX_NUM_UNIQUE_ENTRIES, 1));
    g_assert_true(
        EvictingMap__init(&me, 1.0, 1 << 12, MAX_NUM_UNIQUE_ENTRIES, 1));

    for (uint64_t i = 0; i < TRACE_LENGTH; ++i) {
        uint64_t entry = ZipfianRandom__next(&zrng);
        Olken__access_item(&oracle, entry);
        EvictingMap__access_item(&me, entry);
    }
    struct MissRateCurve oracle_mrc = {0}, mrc = {0};
    MissRateCurve__init_from_histogram(&oracle_mrc, &oracle.histogram);
    MissRateCurve__init_from_histogram(&mrc, &me.histogram);
    double mse = MissRateCurve__mean_squared_error(&oracle_mrc, &mrc);
    LOGGER_INFO("Mean-Squared Error: %lf", mse);
    // NOTE The MSE is as follows for various hashes:
    //      - MurmurHash3: MSE <= 0.02
    //      - splitmix64: MSE <= 0.032
    g_assert_cmpfloat(mse, <=, 0.032);

    ZipfianRandom__destroy(&zrng);
    Olken__destroy(&oracle);
    EvictingMap__destroy(&me);
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
    return EXIT_SUCCESS;
}
