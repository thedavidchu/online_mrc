#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "fixed_size_shards/fixed_size_shards.h"

#include "random/zipfian_random.h"

#include "test/mytester.h"

const bool PRINT_HISTOGRAM = true;
const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;

static bool
access_same_key_five_times()
{
    struct FixedSizeShardsReuseStack me = {0};
    ASSERT_FUNCTION_RETURNS_TRUE(fixed_size_shards_init(&me, 1000, 1, MAX_NUM_UNIQUE_ENTRIES));

    fixed_size_shards_access_item(&me, 0);
    fixed_size_shards_access_item(&me, 0);
    fixed_size_shards_access_item(&me, 0);
    fixed_size_shards_access_item(&me, 0);
    fixed_size_shards_access_item(&me, 0);

    if (me.histogram.histogram[0] != 4000 || me.histogram.false_infinity != 0 ||
        me.histogram.infinity != 1000) {
        fixed_size_shards_print_sparse_histogram(&me);
        assert(0 && "histogram should be {0: 4000, inf: 1000}");
        fixed_size_shards_destroy(&me);
        return false;
    }
    // Skip 0 because we know it should be non-zero
    for (uint64_t i = 1; i < me.histogram.length; ++i) {
        if (me.histogram.histogram[i] != 0) {
            fixed_size_shards_print_sparse_histogram(&me);
            assert(0 && "histogram should be {0: 4000, inf: 1000}");
            fixed_size_shards_destroy(&me);
            return false;
        }
    }

    fixed_size_shards_destroy(&me);
    return true;
}

static bool
trace_test()
{
    const uint64_t trace_length = 1 << 20;
    struct ZipfianRandom zrng = {0};
    struct FixedSizeShardsReuseStack shards = {0};

    ASSERT_FUNCTION_RETURNS_TRUE(zipfian_random_init(&zrng, MAX_NUM_UNIQUE_ENTRIES, 0.5, 0));
    // The maximum trace length is obviously the number of possible unique items
    ASSERT_FUNCTION_RETURNS_TRUE(
        fixed_size_shards_init(&shards, 1000, 100, MAX_NUM_UNIQUE_ENTRIES));

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t key = zipfian_random_next(&zrng);
        fixed_size_shards_access_item(&shards, key);
    }

    if (PRINT_HISTOGRAM) {
        fixed_size_shards_print_sparse_histogram(&shards);
    }

    fixed_size_shards_destroy(&shards);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times());
    ASSERT_FUNCTION_RETURNS_TRUE(trace_test());

    return EXIT_SUCCESS;
}
