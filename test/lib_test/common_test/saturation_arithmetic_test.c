#include <stdbool.h>
#include <stddef.h>

#include <glib.h>

#include "arrays/array_size.h"
#include "math/saturation_arithmetic.h"
#include "test/mytester.h"

struct Oracle {
    size_t a;
    size_t b;
    size_t answer;
};

/// @brief  Test a binary operation that works on two size_t
///         operands.
static bool
test_binary_op_on_size_t(struct Oracle const *const array,
                         size_t const length,
                         bool const test_commute,
                         size_t (*op)(size_t const, size_t const))
{
    for (size_t i = 0; i < length; ++i) {
        g_assert_cmpuint(op(array[i].a, array[i].b), ==, array[i].answer);
        // Test commutativity
        if (test_commute) {
            g_assert_cmpuint(op(array[i].b, array[i].a), ==, array[i].answer);
        }
    }
    return true;
}

static bool
test_saturation_add(void)
{
    struct Oracle stuff[] = {
        // Zero and small numbers
        {0, 0, 0},
        {0, 1, 1},
        {0, 2, 2},
        // Small numbers
        {1, 1, 2},
        {1, 2, 3},
        {2, 2, 4},

        // Zero and near-to SIZE_MAX
        {0, SIZE_MAX - 1, SIZE_MAX - 1},
        {0, SIZE_MAX - 2, SIZE_MAX - 2},
        // Small numbers an near-to SIZE_MAX
        {1, SIZE_MAX - 1, SIZE_MAX},
        {1, SIZE_MAX - 2, SIZE_MAX - 1},
        {2, SIZE_MAX - 1, SIZE_MAX},
        {2, SIZE_MAX - 2, SIZE_MAX},
        // Near-to SIZE_MAX
        {SIZE_MAX - 1, SIZE_MAX - 1, SIZE_MAX},
        {SIZE_MAX - 1, SIZE_MAX - 2, SIZE_MAX},
        {SIZE_MAX - 2, SIZE_MAX - 2, SIZE_MAX},

        // Zero and SIZE_MAX
        {0, SIZE_MAX, SIZE_MAX},
        // Small numbers and SIZE_MAX
        {1, SIZE_MAX, SIZE_MAX},
        {2, SIZE_MAX, SIZE_MAX},
        // Near-to SIZE_MAX and SIZE_MAX
        {SIZE_MAX - 1, SIZE_MAX, SIZE_MAX},
        {SIZE_MAX - 2, SIZE_MAX, SIZE_MAX},
        // SIZE_MAX
        {SIZE_MAX, SIZE_MAX, SIZE_MAX},
    };
    return test_binary_op_on_size_t(stuff,
                                    ARRAY_SIZE(stuff),
                                    true,
                                    saturation_add);
}

static bool
test_saturation_multiply(void)
{
    struct Oracle stuff[] = {
        // Zero and small numbers
        {0, 0, 0},
        {0, 1, 0},
        {0, 2, 0},
        // Small numbers
        {1, 1, 1},
        {1, 2, 2},
        {2, 2, 4},

        // Zero and near-to SIZE_MAX
        {0, SIZE_MAX - 1, 0},
        {0, SIZE_MAX - 2, 0},
        // Small numbers an near-to SIZE_MAX
        {1, SIZE_MAX - 1, SIZE_MAX - 1},
        {1, SIZE_MAX - 2, SIZE_MAX - 2},
        {2, SIZE_MAX - 1, SIZE_MAX},
        {2, SIZE_MAX - 2, SIZE_MAX},
        // Near-to SIZE_MAX
        {SIZE_MAX - 1, SIZE_MAX - 1, SIZE_MAX},
        {SIZE_MAX - 1, SIZE_MAX - 2, SIZE_MAX},
        {SIZE_MAX - 2, SIZE_MAX - 2, SIZE_MAX},

        // Zero and SIZE_MAX
        {0, SIZE_MAX, 0},
        // Small numbers and SIZE_MAX
        {1, SIZE_MAX, SIZE_MAX},
        {2, SIZE_MAX, SIZE_MAX},
        // Near-to SIZE_MAX and SIZE_MAX
        {SIZE_MAX - 1, SIZE_MAX, SIZE_MAX},
        {SIZE_MAX - 2, SIZE_MAX, SIZE_MAX},
        // SIZE_MAX
        {SIZE_MAX, SIZE_MAX, SIZE_MAX},
    };

    return test_binary_op_on_size_t(stuff,
                                    ARRAY_SIZE(stuff),
                                    true,
                                    saturation_multiply);
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_saturation_add());
    ASSERT_FUNCTION_RETURNS_TRUE(test_saturation_multiply());
    return 0;
}
