#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mimir/buckets.h"
#include "mimir/mimir.h"

#include "random/zipfian_random.h"

#include "test/mytester.h"

const bool PRINT_HISTOGRAM = true;
const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;

static bool
access_same_key_five_times()
{
    struct Mimir me = {0};
    // NOTE I reduced the max_num_unique_entries to reduce the runtime.
    ASSERT_TRUE_OR_CLEANUP(mimir__init(&me, 10, 1000, MIMIR_ROUNDER),
                           printf("No cleanup required\n"));

    mimir__access_item(&me, 0);
    mimir_buckets__validate(&me.buckets);
    mimir__access_item(&me, 0);
    mimir_buckets__validate(&me.buckets);
    mimir__access_item(&me, 0);
    mimir_buckets__validate(&me.buckets);
    mimir__access_item(&me, 0);
    mimir_buckets__validate(&me.buckets);
    mimir__access_item(&me, 0);
    mimir_buckets__validate(&me.buckets);

    if (me.histogram.histogram[0] != 4.0 ||
        me.histogram.false_infinity != 0.0 || me.histogram.infinity != 1) {
        mimir__print_sparse_histogram(&me);
        assert(0 && "histogram should be {0: 4, inf: 1}");
        mimir__destroy(&me);
        return false;
    }
    // Skip 0 because we know it should be non-zero
    for (uint64_t i = 1; i < me.histogram.length; ++i) {
        if (me.histogram.histogram[i] != 0.0) {
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
trace_test()
{
    const uint64_t trace_length = 1 << 20;
    struct ZipfianRandom zrng = {0};
    struct Mimir shards = {0};

    ASSERT_FUNCTION_RETURNS_TRUE(
        zipfian_random__init(&zrng, MAX_NUM_UNIQUE_ENTRIES, 0.5, 0));
    // The maximum trace length is obviously the number of possible unique items
    // NOTE I reduced the max_num_unique_entries to reduce the runtime. Doing so
    //      absolutely demolishes the accuracy as well. Oh well, now this test
    //      is kind of useless!
    ASSERT_FUNCTION_RETURNS_TRUE(
        mimir__init(&shards, 1000, 1000, MIMIR_ROUNDER));

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t key = zipfian_random__next(&zrng);
        mimir__access_item(&shards, key);
    }

    if (PRINT_HISTOGRAM) {
        mimir__print_sparse_histogram(&shards);
    }

    mimir__destroy(&shards);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times());
    ASSERT_FUNCTION_RETURNS_TRUE(trace_test());

    return EXIT_SUCCESS;
}
