#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mrc_reuse_stack/fixed_size_shards.h"
#include "mrc_reuse_stack/olken.h"

#include "random/zipfian_random.h"

#include "test/mytester.h"

#define PRINT_HISTOGRAM true

bool
olken_access_same_key_five_times()
{
    struct OlkenReuseStack olken_reuse_stack = {0};
    ASSERT_TRUE_OR_CLEANUP(olken_reuse_stack_init(&olken_reuse_stack),
                           printf("No cleanup required\n"));

    olken_reuse_stack_access_item(&olken_reuse_stack, 0);
    olken_reuse_stack_access_item(&olken_reuse_stack, 0);
    olken_reuse_stack_access_item(&olken_reuse_stack, 0);
    olken_reuse_stack_access_item(&olken_reuse_stack, 0);
    olken_reuse_stack_access_item(&olken_reuse_stack, 0);

    if (olken_reuse_stack.histogram.histogram[0] != 4 ||
        olken_reuse_stack.histogram.infinity != 1) {
        printf("%p %p\n", &olken_reuse_stack, &olken_reuse_stack.histogram);
        olken_reuse_stack_print_sparse_histogram(&olken_reuse_stack);
        assert(0 && "histogram should be {0: 4, inf: 1}");
        olken_reuse_stack_destroy(&olken_reuse_stack);
        return false;
    }

    olken_reuse_stack_destroy(&olken_reuse_stack);
    return true;
}

bool
fixed_size_shards_trace_test()
{
    const uint64_t trace_length = 1 << 20;
    struct ZipfianRandom zrng = {0};
    struct FixedSizeShardsReuseStack shards = {0};

    ASSERT_FUNCTION_RETURNS_TRUE(zipfian_random_init(&zrng, 1 << 15, 0.5, 0));
    // The maximum trace length is obviously the number of possible unique items
    ASSERT_FUNCTION_RETURNS_TRUE(fixed_size_shards_init(&shards, 1000, 1000, 1 << 15));

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
    ASSERT_FUNCTION_RETURNS_TRUE(olken_access_same_key_five_times());
    ASSERT_FUNCTION_RETURNS_TRUE(fixed_size_shards_trace_test());

    return EXIT_SUCCESS;
}
