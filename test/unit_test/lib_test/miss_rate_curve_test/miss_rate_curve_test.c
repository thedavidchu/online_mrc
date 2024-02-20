#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "histogram/basic_histogram.h"
#include "logger/logger.h"
#include "miss_rate_curve/basic_miss_rate_curve.h"

#include "math/doubles_are_equal.h"
#include "test/mytester.h"
#include "unused/mark_unused.h"

// NOTE These are 100 random integers in the range 0..=10. Generated with Python
//      script:
//      import random; x = [random.randint(0, 10) for _ in range(100)]; print(x)
const uint64_t random_values_0_to_11[100] = {
    2, 3,  2, 5,  0, 1, 7, 9, 4, 2,  10, 3, 1,  10, 10, 5, 10, 6,  5, 0,
    6, 4,  2, 9,  7, 2, 2, 5, 3, 9,  6,  0, 1,  1,  6,  1, 6,  7,  5, 0,
    0, 10, 8, 3,  1, 2, 6, 7, 3, 10, 8,  6, 10, 6,  6,  2, 6,  0,  7, 9,
    6, 10, 1, 10, 2, 6, 2, 7, 8, 8,  6,  0, 7,  3,  1,  1, 2,  10, 3, 10,
    5, 5,  0, 7,  9, 8, 0, 7, 6, 9,  4,  9, 4,  8,  3,  6, 5,  3,  2, 9};

static bool
exact_match(struct BasicMissRateCurve *lhs, struct BasicMissRateCurve *rhs)
{
    assert(lhs != NULL && rhs != NULL && lhs->miss_rate != NULL &&
           rhs->miss_rate != NULL);
    if (lhs->length != rhs->length) {
        return false;
    }
    for (uint64_t i = 0; i < lhs->length; ++i) {
        if (!doubles_are_equal(lhs->miss_rate[i], rhs->miss_rate[i])) {
            LOGGER_ERROR("Mismatch in iteration %" PRIu64 ": %lf != %lf\n",
                         i,
                         lhs->miss_rate[i],
                         rhs->miss_rate[i]);
            return false;
        }
    }
    return true;
}

static bool
test_miss_rate_curve_for_basic_histogram(void)
{
    // NOTE Histogram oracle generated by getting the random values generated
    //      above and running:
    //      y = [x.count(i) for i in range(10)]; print(y); z = x.count(10);
    //      print(z)
    uint64_t hist_vals[] = {9, 9, 12, 9, 4, 8, 15, 9, 6, 8};
    struct BasicHistogram basic_hist = {
        .histogram = hist_vals,
        .length = 10,
        .false_infinity = 11,
        .infinity = 3,
        .running_sum = 103,
    };

    struct BasicMissRateCurve mrc = {0};
    struct BasicMissRateCurve mrc_from_file = {0};
    ASSERT_TRUE_WITHOUT_CLEANUP(
        basic_miss_rate_curve__init_from_basic_histogram(&mrc, &basic_hist));
    ASSERT_TRUE_OR_CLEANUP(
        basic_miss_rate_curve__write_binary_to_file(&mrc, "mrc.bin"),
        /*cleanup=*/basic_miss_rate_curve__destroy(&mrc));
    ASSERT_TRUE_OR_CLEANUP(basic_miss_rate_curve__init_from_file(&mrc_from_file,
                                                                 "mrc.bin",
                                                                 mrc.length),
                           /*cleanup=*/basic_miss_rate_curve__destroy(&mrc));
    ASSERT_TRUE_OR_CLEANUP(exact_match(&mrc, &mrc_from_file),
                           /*cleanup=*/basic_miss_rate_curve__destroy(&mrc));
    ASSERT_TRUE_OR_CLEANUP(
        basic_miss_rate_curve__mean_squared_error(&mrc, &mrc_from_file) == 0.0,
        /*cleanup=*/basic_miss_rate_curve__destroy(&mrc));
    basic_miss_rate_curve__destroy(&mrc);
    basic_miss_rate_curve__destroy(&mrc_from_file);
    return true;
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    ASSERT_FUNCTION_RETURNS_TRUE(test_miss_rate_curve_for_basic_histogram());
    return 0;
}