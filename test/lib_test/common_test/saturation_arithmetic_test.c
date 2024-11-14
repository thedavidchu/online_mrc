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

bool
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

    for (size_t i = 0; i < ARRAY_SIZE(stuff); ++i) {
        g_assert_cmpuint(saturation_add(stuff[i].a, stuff[i].b),
                         ==,
                         stuff[i].answer);
        // Test commutativity
        g_assert_cmpuint(saturation_add(stuff[i].b, stuff[i].a),
                         ==,
                         stuff[i].answer);
    }
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_saturation_add());
    return 0;
}
