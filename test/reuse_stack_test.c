#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "mrc_reuse_stack/olken.h"

#include "test/mytester.h"

bool
access_same_key_five_times()
{
    struct OlkenReuseStack *olken_reuse_stack = olken_reuse_stack_new();

    olken_reuse_stack_access_item(olken_reuse_stack, 0);
    olken_reuse_stack_access_item(olken_reuse_stack, 0);
    olken_reuse_stack_access_item(olken_reuse_stack, 0);
    olken_reuse_stack_access_item(olken_reuse_stack, 0);
    olken_reuse_stack_access_item(olken_reuse_stack, 0);

    if (olken_reuse_stack->histogram[0] != 4 || olken_reuse_stack->infinite_distance != 1) {
        printf("%p %p\n", olken_reuse_stack, olken_reuse_stack->histogram);
        olken_reuse_stack_print_sparse_histogram(olken_reuse_stack);
        assert(0 && "histogram should be {0: 4, inf: 1}");
        olken_reuse_stack_free(olken_reuse_stack);
        return false;
    }

    olken_reuse_stack_free(olken_reuse_stack);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(access_same_key_five_times());

    return EXIT_SUCCESS;
}
