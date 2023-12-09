#include <assert.h>
#include <stdbool.h>

#include "reuse_stack/reuse_stack.h"

bool
access_same_key_five_times()
{
    struct ReuseStack *reuse_stack = reuse_stack_new();

    reuse_stack_access_item(reuse_stack, 0);
    reuse_stack_access_item(reuse_stack, 0);
    reuse_stack_access_item(reuse_stack, 0);
    reuse_stack_access_item(reuse_stack, 0);
    reuse_stack_access_item(reuse_stack, 0);

    if (reuse_stack->histogram[0] != 4 || reuse_stack->infinite_distance != 1) {
        assert(0 && "histogram should be {0: 4, inf: 1}");
        reuse_stack_free(reuse_stack);
        return false;
    }

    reuse_stack_free(reuse_stack);
    return true;
}

int
main(void)
{
    bool r = false;
    r = access_same_key_five_times();
    if (!r) {
        assert(0 && "should have passed lol");
    }
    return 0;
}