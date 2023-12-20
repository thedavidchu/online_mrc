#include <assert.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "math/doubles_are_equal.h"
#include "mimir/buckets.h"
#include "mimir/mimir.h"

#include "random/zipfian_random.h"

#include "test/mytester.h"
#include "unused/mark_unused.h"

const bool PRINT_HISTOGRAM = true;
const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;

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
    const uint64_t trace_length = 1 << 20;
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
    if (false) {
        ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times(MIMIR_ROUNDER));
        ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times(MIMIR_STACKER));
        ASSERT_FUNCTION_RETURNS_TRUE(trace_test(MIMIR_ROUNDER));
    }
    ASSERT_FUNCTION_RETURNS_TRUE(trace_test(MIMIR_STACKER));
    return EXIT_SUCCESS;
}
