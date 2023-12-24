#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "arrays/array_size.h"
#include "fixed_rate_shards/parda_fixed_rate_shards.h"
#include "logger/logger.h"
#include "miss_rate_curve/basic_miss_rate_curve.h"
#include "olken/olken.h"
#include "parda.h"
#include "random/zipfian_random.h"
#include "test/mytester.h"
#include "unused/mark_unused.h"

const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;
const uint64_t trace_length = 1 << 20;
const double ZIPFIAN_RANDOM_SKEW = 0.99;

static bool
access_same_key_five_times(void)
{
    EntryType entries[5] = {0, 0, 0, 0, 0};
    struct OlkenReuseStack oracle = {0};
    struct FixedRateShards me = {0};

    // The maximum trace length is obviously the number of possible unique items
    ASSERT_TRUE_WITHOUT_CLEANUP(olken__init(&oracle, MAX_NUM_UNIQUE_ENTRIES));
    ASSERT_TRUE_OR_CLEANUP(parda_fixed_rate_shards__init(&me, 1),
                           olken__destroy(&oracle));

    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        uint64_t entry = entries[i];
        olken__access_item(&oracle, entry);
        parda_fixed_rate_shards__access_item(&me, entry);
    }
    struct BasicMissRateCurve oracle_mrc = {0}, mrc = {0};
    basic_miss_rate_curve__init_from_basic_histogram(&oracle_mrc,
                                                     &oracle.histogram);
    basic_miss_rate_curve__init_from_parda_histogram(
        &mrc,
        nbuckets,
        me.program_data.histogram,
        me.current_time_stamp,
        me.program_data.histogram[B_OVFL]);
    double mse = basic_miss_rate_curve__mean_squared_error(&oracle_mrc, &mrc);
    LOGGER_INFO("Mean-Squared Error: %lf", mse);
    // HACK Ahh, the comma operator. It allows me to do this wonderful little
    //      macro hack that could have otherwise required no brackets around the
    //      cleanup expression in the macro definition and for me to use a
    //      semicolon. But instead, I rely on the fact that macros have to match
    //      round parentheses. The more I code in C, the more I realize how much
    //      a safer language with more intelligent macros/generics would be.
    ASSERT_TRUE_OR_CLEANUP(
        mse <= 0.000001,
        (olken__destroy(&oracle), parda_fixed_rate_shards__destroy(&me)));

    olken__destroy(&oracle);
    parda_fixed_rate_shards__destroy(&me);
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
    struct OlkenReuseStack oracle = {0};
    struct FixedRateShards me = {0};

    // The maximum trace length is obviously the number of possible unique items
    ASSERT_TRUE_WITHOUT_CLEANUP(olken__init(&oracle, MAX_NUM_UNIQUE_ENTRIES));
    ASSERT_TRUE_OR_CLEANUP(parda_fixed_rate_shards__init(&me, 1),
                           olken__destroy(&oracle));

    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        uint64_t entry = entries[i];
        olken__access_item(&oracle, entry);
        parda_fixed_rate_shards__access_item(&me, entry);
    }
    struct BasicMissRateCurve oracle_mrc = {0}, mrc = {0};
    basic_miss_rate_curve__init_from_basic_histogram(&oracle_mrc,
                                                     &oracle.histogram);
    basic_miss_rate_curve__init_from_parda_histogram(
        &mrc,
        nbuckets,
        me.program_data.histogram,
        me.current_time_stamp,
        me.program_data.histogram[B_OVFL]);
    double mse = basic_miss_rate_curve__mean_squared_error(&oracle_mrc, &mrc);
    LOGGER_INFO("Mean-Squared Error: %lf", mse);
    // HACK Ahh, the comma operator. It allows me to do this wonderful little
    //      macro hack that could have otherwise required no brackets around the
    //      cleanup expression in the macro definition and for me to use a
    //      semicolon. But instead, I rely on the fact that macros have to match
    //      round parentheses. The more I code in C, the more I realize how much
    //      a safer language with more intelligent macros/generics would be.
    ASSERT_TRUE_OR_CLEANUP(
        mse <= 0.000001,
        (olken__destroy(&oracle), parda_fixed_rate_shards__destroy(&me)));

    olken__destroy(&oracle);
    parda_fixed_rate_shards__destroy(&me);
    return true;
}

static bool
long_accuracy_trace_test(void)
{
    struct ZipfianRandom zrng = {0};
    struct OlkenReuseStack oracle = {0};
    struct FixedRateShards me = {0};

    ASSERT_TRUE_WITHOUT_CLEANUP(zipfian_random__init(&zrng,
                                                     MAX_NUM_UNIQUE_ENTRIES,
                                                     ZIPFIAN_RANDOM_SKEW,
                                                     0));
    // The maximum trace length is obviously the number of possible unique items
    ASSERT_TRUE_WITHOUT_CLEANUP(olken__init(&oracle, MAX_NUM_UNIQUE_ENTRIES));
    ASSERT_TRUE_OR_CLEANUP(parda_fixed_rate_shards__init(&me, 1000),
                           olken__destroy(&oracle));

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t entry = zipfian_random__next(&zrng);
        olken__access_item(&oracle, entry);
        parda_fixed_rate_shards__access_item(&me, entry);
    }
    struct BasicMissRateCurve oracle_mrc = {0}, mrc = {0};
    basic_miss_rate_curve__init_from_basic_histogram(&oracle_mrc,
                                                     &oracle.histogram);
    basic_miss_rate_curve__init_from_parda_histogram(
        &mrc,
        nbuckets,
        me.program_data.histogram,
        me.current_time_stamp,
        me.program_data.histogram[B_OVFL]);
    double mse = basic_miss_rate_curve__mean_squared_error(&oracle_mrc, &mrc);
    LOGGER_INFO("Mean-Squared Error: %lf", mse);
    // HACK Ahh, the comma operator. It allows me to do this wonderful little
    //      macro hack that could have otherwise required no brackets around the
    //      cleanup expression in the macro definition and for me to use a
    //      semicolon. But instead, I rely on the fact that macros have to match
    //      round parentheses. The more I code in C, the more I realize how much
    //      a safer language with more intelligent macros/generics would be.
    ASSERT_TRUE_OR_CLEANUP(
        mse <= 0.04,
        (olken__destroy(&oracle), parda_fixed_rate_shards__destroy(&me)));

    olken__destroy(&oracle);
    parda_fixed_rate_shards__destroy(&me);
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
