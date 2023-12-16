#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "olken/olken.h"

#include "random/zipfian_random.h"

#include "test/mytester.h"

const bool PRINT_HISTOGRAM = true;
const uint64_t MAX_NUM_UNIQUE_ENTRIES = 1 << 20;

static bool
access_same_key_five_times()
{
    struct OlkenReuseStack me = {0};
    ASSERT_TRUE_OR_CLEANUP(olken___init(&me, MAX_NUM_UNIQUE_ENTRIES),
                           printf("No cleanup required\n"));

    olken___access_item(&me, 0);
    olken___access_item(&me, 0);
    olken___access_item(&me, 0);
    olken___access_item(&me, 0);
    olken___access_item(&me, 0);

    if (me.histogram.histogram[0] != 4 || me.histogram.false_infinity != 0 ||
        me.histogram.infinity != 1) {
        olken___print_sparse_histogram(&me);
        assert(0 && "histogram should be {0: 4, inf: 1}");
        olken___destroy(&me);
        return false;
    }
    // Skip 0 because we know it should be non-zero
    for (uint64_t i = 1; i < me.histogram.length; ++i) {
        if (me.histogram.histogram[i] != 0) {
            olken___print_sparse_histogram(&me);
            assert(0 && "histogram should be {0: 4, inf: 1}");
            olken___destroy(&me);
            return false;
        }
    }

    olken___destroy(&me);
    return true;
}

static bool
trace_test()
{
    const uint64_t trace_length = 1 << 20;
    struct ZipfianRandom zrng = {0};
    struct OlkenReuseStack me = {0};

    ASSERT_FUNCTION_RETURNS_TRUE(zipfian_random__init(&zrng, MAX_NUM_UNIQUE_ENTRIES, 0.5, 0));
    // The maximum trace length is obviously the number of possible unique items
    ASSERT_FUNCTION_RETURNS_TRUE(olken___init(&me, MAX_NUM_UNIQUE_ENTRIES));

    for (uint64_t i = 0; i < trace_length; ++i) {
        uint64_t key = zipfian_random__next(&zrng);
        olken___access_item(&me, key);
    }

    if (PRINT_HISTOGRAM) {
        olken___print_sparse_histogram(&me);
    }

    olken___destroy(&me);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times());
    ASSERT_FUNCTION_RETURNS_TRUE(trace_test());

    return EXIT_SUCCESS;
}
