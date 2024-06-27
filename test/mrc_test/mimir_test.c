#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "arrays/array_size.h"
#include "histogram/fractional_histogram.h"
#include "logger/logger.h"
#include "mimir/buckets.h"
#include "mimir/mimir.h"
#include "mimir/private_buckets.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "random/zipfian_random.h"
#include "test/mytester.h"
#include "unused/mark_unused.h"

const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;
const double ZIPFIAN_RANDOM_SKEW = 0.99;
const uint64_t TRACE_LENGTH = 1 << 20;

////////////////////////////////////////////////////////////////////////////////
/// UNIT TESTS
////////////////////////////////////////////////////////////////////////////////

static void
tester_refresh_buckets(struct MimirBuckets *me)
{
    const uint64_t original_weighted_sum_of_bucket_indices =
        0 * 100 + 1 * 10 + 2 * 20 + 3 * 30 + 4 * 40 + 5 * 50 + 6 * 60 + 7 * 70 +
        8 * 80 + 9 * 90;
    const uint64_t original_buckets[10] =
        {100, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    const struct MimirBuckets reset_target = {
        .buckets = me->buckets,
        .num_buckets = 10,
        .newest_bucket = 9,
        .oldest_bucket = 0,
        .num_unique_entries = 550,
        .sum_of_bucket_indices = original_weighted_sum_of_bucket_indices};

    // Do the reset
    for (uint64_t i = 0; i < ARRAY_SIZE(original_buckets); ++i) {
        reset_target.buckets[i] = original_buckets[i];
    }
    *me = reset_target;
}

static bool
tester_ensure_buckets_match(struct MimirBuckets *me,
                            const uint64_t *oracle_buckets)
{
    for (uint64_t i = 0; i < me->num_buckets; ++i) {
        g_assert_true(me->buckets[i] == oracle_buckets[i]);
    }
    return true;
}

static bool
test_mimir_buckets(void)
{
    struct MimirBuckets me = {0};
    g_assert_true(MimirBuckets__init(&me, 10));
    tester_refresh_buckets(&me);
    g_assert_true(MimirBuckets__validate(&me));
    g_assert_true(9 == MimirBuckets__get_newest_bucket_index(&me));
    g_assert_true(MimirBuckets__validate(&me));
    g_assert_true(90 == get_newest_bucket_size(&me));
    g_assert_true(55 == get_average_num_entries_per_bucket(&me));
    g_assert_true(2850 == count_weighted_sum_of_bucket_indices(&me));

    // Test Rounder aging
    g_assert_true(5 == MimirBuckets__get_average_bucket_index(&me));
    const uint64_t oracle_buckets_rounder[20][10] = {
        {100, 10, 20, 30, 40, 50, 60, 70, 80, 90},
        {0, 110, 20, 30, 40, 50, 60, 70, 80, 90},
        {0, 0, 130, 30, 40, 50, 60, 70, 80, 90},
        {0, 0, 0, 160, 40, 50, 60, 70, 80, 90},
        {0, 0, 0, 0, 200, 50, 60, 70, 80, 90},
        {0, 0, 0, 0, 0, 250, 60, 70, 80, 90},
        {0, 0, 0, 0, 0, 0, 310, 70, 80, 90},
        {0, 0, 0, 0, 0, 0, 0, 380, 80, 90},
        {0, 0, 0, 0, 0, 0, 0, 0, 460, 90},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 550},
        {550, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 550, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 550, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 550, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 550, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 550, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 550, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 550, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 550, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 550},
    };
    for (uint64_t i = 0; i < ARRAY_SIZE(oracle_buckets_rounder); ++i) {
        g_assert_true(
            tester_ensure_buckets_match(&me, oracle_buckets_rounder[i]));
        g_assert_true(MimirBuckets__rounder_aging_policy(&me));
    }
    g_assert_true(MimirBuckets__validate(&me));

    // Test Stacker aging
    tester_refresh_buckets(&me);
    MimirBuckets__stacker_aging_policy(&me, 5);
    const uint64_t oracle_buckets_stacker[1][10] = {
        {100, 10, 20, 30, 90, 60, 70, 80, 90, 0}};
    g_assert_true(tester_ensure_buckets_match(&me, oracle_buckets_stacker[0]));
    g_assert_true(MimirBuckets__validate(&me));

    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// INTEGRATION TESTS
////////////////////////////////////////////////////////////////////////////////

static bool
access_same_key_five_times(enum MimirAgingPolicy aging_policy)
{
    EntryType entries[5] = {0, 0, 0, 0, 0};
    double hist_bkt_oracle[11] =
        {4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    struct FractionalHistogram histogram_oracle = {
        .histogram = hist_bkt_oracle,
        .num_bins = ARRAY_SIZE(hist_bkt_oracle),
        .bin_size = 1,
        .false_infinity = 0.0,
        .infinity = 1,
        .running_sum = ARRAY_SIZE(entries),
    };
    struct Mimir me = {0};
    g_assert_true(
        Mimir__init(&me, 10, histogram_oracle.num_bins, 1, aging_policy));
    for (uint64_t i = 0; i < ARRAY_SIZE(entries); ++i) {
        Mimir__access_item(&me, 0);
        Mimir__validate(&me);
    }
    FractionalHistogram__print_as_json(&me.histogram);
    FractionalHistogram__print_as_json(&histogram_oracle);
    g_assert_true(
        FractionalHistogram__exactly_equal(&me.histogram, &histogram_oracle));
    Mimir__destroy(&me);
    return true;
}

static bool
long_accuracy_trace_test(enum MimirAgingPolicy aging_policy)
{
    struct ZipfianRandom zrng = {0};
    struct Olken oracle = {0};
    struct Mimir me = {0};
    g_assert_true(ZipfianRandom__init(&zrng,
                                      MAX_NUM_UNIQUE_ENTRIES,
                                      ZIPFIAN_RANDOM_SKEW,
                                      0));
    // The maximum trace length is obviously the number of possible unique items
    g_assert_true(Olken__init(&oracle, MAX_NUM_UNIQUE_ENTRIES, 100));
    g_assert_true(
        Mimir__init(&me, 1000, MAX_NUM_UNIQUE_ENTRIES, 100, aging_policy));
    Mimir__validate(&me);
    // NOTE I reduced the max_num_unique_entries to reduce the runtime. Doing so
    //      absolutely demolishes the accuracy as well. Oh well, now this test
    //      is kind of useless!
    for (uint64_t i = 0; i < TRACE_LENGTH; ++i) {
        uint64_t entry = ZipfianRandom__next(&zrng);
        Olken__access_item(&oracle, entry);
        Mimir__access_item(&me, entry);
    }
    struct MissRateCurve oracle_mrc = {0}, mrc = {0};
    MissRateCurve__init_from_histogram(&oracle_mrc, &oracle.histogram);
    MissRateCurve__init_from_fractional_histogram(&mrc, &me.histogram);
    double mse = MissRateCurve__mean_squared_error(&oracle_mrc, &mrc);
    LOGGER_INFO("Mean-Squared Error: %lf", mse);
    g_assert_cmpfloat(mse, <=, 0.003);

    ZipfianRandom__destroy(&zrng);
    Olken__destroy(&oracle);
    Mimir__destroy(&me);
    return true;
}

static void
run_unit_tests(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_mimir_buckets());
}

static void
run_rounder_tests(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times(MIMIR_ROUNDER));
    ASSERT_FUNCTION_RETURNS_TRUE(long_accuracy_trace_test(MIMIR_ROUNDER));
}

static void
run_stacker_tests(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times(MIMIR_STACKER));
    ASSERT_FUNCTION_RETURNS_TRUE(long_accuracy_trace_test(MIMIR_STACKER));
}

int
main(int argc, char **argv)
{
    char *policy = NULL;
    if (argc == 1) {
        policy = "all";
    } else if (argc == 2) {
        policy = argv[1];
    } else {
        // We don't count the first argument (i.e. the executable name)
        // as an argument.
        LOGGER_ERROR("expected at most 1 argument, got %d", argc - 1);
        return EXIT_FAILURE;
    }
    LOGGER_INFO("policy: %s", policy);

    if (argc == 1 || strcmp(policy, "all") == 0) {
        LOGGER_INFO("1", policy);
        run_unit_tests();
        LOGGER_INFO("2", policy);
        run_rounder_tests();
        LOGGER_INFO("3", policy);
        run_stacker_tests();
        LOGGER_INFO("4", policy);
    } else if (strcmp(policy, "rounder") == 0) {
        run_rounder_tests();
    } else if (strcmp(policy, "stacker") == 0) {
        run_stacker_tests();
    } else if (strcmp(policy, "unit") == 0) {
        run_unit_tests();
    } else {
        LOGGER_ERROR("unrecognized argument '%s', expected 'all', 'rounder', "
                     "'stacker', or 'unit'.",
                     policy);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
