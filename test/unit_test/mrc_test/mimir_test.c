#include <assert.h>
#include <bits/stdint-uintn.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "math/doubles_are_equal.h"
#include "mimir/buckets.h"
#include "mimir/mimir.h"

#include "mimir/private_buckets.h"
#include "random/zipfian_random.h"

#include "arrays/array_size.h"
#include "test/mytester.h"
#include "unused/mark_unused.h"

const bool PRINT_HISTOGRAM = true;
const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;

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
        ASSERT_TRUE_WITHOUT_CLEANUP(me->buckets[i] == oracle_buckets[i]);
    }
    return true;
}

static bool
test_mimir_buckets(void)
{
    struct MimirBuckets me = {0};
    ASSERT_TRUE_WITHOUT_CLEANUP(mimir_buckets__init(&me, 10));
    tester_refresh_buckets(&me);
    ASSERT_TRUE_WITHOUT_CLEANUP(mimir_buckets__validate(&me));
    ASSERT_TRUE_WITHOUT_CLEANUP(9 ==
                                mimir_buckets__get_newest_bucket_index(&me));
    ASSERT_TRUE_WITHOUT_CLEANUP(mimir_buckets__validate(&me));
    ASSERT_TRUE_WITHOUT_CLEANUP(90 == get_newest_bucket_size(&me));
    ASSERT_TRUE_WITHOUT_CLEANUP(55 == get_average_num_entries_per_bucket(&me));
    ASSERT_TRUE_WITHOUT_CLEANUP(2850 ==
                                count_weighted_sum_of_bucket_indices(&me));

    // Test Rounder aging
    ASSERT_TRUE_WITHOUT_CLEANUP(5 ==
                                mimir_buckets__get_average_bucket_index(&me));
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
        ASSERT_TRUE_WITHOUT_CLEANUP(
            tester_ensure_buckets_match(&me, oracle_buckets_rounder[i]));
        ASSERT_TRUE_WITHOUT_CLEANUP(mimir_buckets__rounder_aging_policy(&me));
    }
    ASSERT_TRUE_WITHOUT_CLEANUP(mimir_buckets__validate(&me));

    // Test Stacker aging
    tester_refresh_buckets(&me);
    mimir_buckets__stacker_aging_policy(&me, 5);
    const uint64_t oracle_buckets_stacker[1][10] = {
        {100, 10, 20, 30, 90, 60, 70, 80, 90, 0}};
    ASSERT_TRUE_WITHOUT_CLEANUP(
        tester_ensure_buckets_match(&me, oracle_buckets_stacker[0]));
    ASSERT_TRUE_WITHOUT_CLEANUP(mimir_buckets__validate(&me));

    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// INTEGRATION TESTS
////////////////////////////////////////////////////////////////////////////////

static bool
access_same_key_five_times(enum MimirAgingPolicy aging_policy)
{
    struct Mimir me = {0};
    // NOTE I reduced the max_num_unique_entries to reduce the runtime.
    ASSERT_TRUE_WITHOUT_CLEANUP(mimir__init(&me, 10, 1000, aging_policy));

    mimir__access_item(&me, 0);
    mimir__validate(&me);
    mimir__access_item(&me, 0);
    mimir__validate(&me);
    mimir__access_item(&me, 0);
    mimir__validate(&me);
    mimir__access_item(&me, 0);
    mimir__validate(&me);
    mimir__access_item(&me, 0);
    mimir__validate(&me);

    if (!doubles_are_equal(me.histogram.histogram[0], 4.0) ||
        !doubles_are_equal(me.histogram.false_infinity, 0.0) ||
        me.histogram.infinity != 1) {
        mimir__print_sparse_histogram(&me);
        assert(0 && "histogram should be {0: 4, inf: 1}");
        mimir__destroy(&me);
        return false;
    }
    // Skip 0 because we know it should be non-zero
    for (uint64_t i = 1; i < me.histogram.length; ++i) {
        if (!doubles_are_equal(me.histogram.histogram[i], 0.0)) {
            mimir__print_sparse_histogram(&me);
            assert(0 && "histogram should be {0: 4, inf: 1}");
            mimir__destroy(&me);
            return false;
        }
    }

    mimir__destroy(&me);
    return true;
}

static bool
trace_test(enum MimirAgingPolicy aging_policy)
{
    const uint64_t trace_length = 1 << 15;
    struct ZipfianRandom zrng = {0};
    struct Mimir me = {0};

    ASSERT_TRUE_WITHOUT_CLEANUP(
        zipfian_random__init(&zrng, MAX_NUM_UNIQUE_ENTRIES, 0.5, 0));
    // The maximum trace length is obviously the number of possible unique items
    // NOTE I reduced the max_num_unique_entries to reduce the runtime. Doing so
    //      absolutely demolishes the accuracy as well. Oh well, now this test
    //      is kind of useless!
    ASSERT_TRUE_WITHOUT_CLEANUP(mimir__init(&me, 1000, 1000, aging_policy));
    mimir__validate(&me);

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t key = zipfian_random__next(&zrng);
        mimir__access_item(&me, key);
        mimir__validate(&me);
    }

    if (PRINT_HISTOGRAM) {
        mimir__print_sparse_histogram(&me);
    }

    mimir__destroy(&me);
    return true;
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    // Unit tests
    ASSERT_FUNCTION_RETURNS_TRUE(test_mimir_buckets());
    // // Integration tests
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times(MIMIR_ROUNDER));
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times(MIMIR_STACKER));
    ASSERT_FUNCTION_RETURNS_TRUE(trace_test(MIMIR_ROUNDER));
    ASSERT_FUNCTION_RETURNS_TRUE(trace_test(MIMIR_STACKER));
    return EXIT_SUCCESS;
}
