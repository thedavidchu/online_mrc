#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "histogram/basic_histogram.h"
#include "histogram/fractional_histogram.h"

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

#define ASSERT_TRUE_OR_RETURN_FALSE(r, msg, histogram_ptr)                     \
    do {                                                                       \
        if (!(r)) {                                                            \
            /* Clean up resources */                                           \
            BasicHistogram__destroy((histogram_ptr));                          \
            printf("[ERROR] %s:%d %s\n", __FILE__, __LINE__, (msg));           \
            /* NOTE This assertion is for debugging purposes so that we have a \
            finer grain understanding of where the failure occurred. */        \
            assert((r) && (msg));                                              \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool
test_basic_histogram(void)
{
    struct BasicHistogram hist = {0};

    bool r = BasicHistogram__init(&hist, 10);
    if (!r) {
        return false;
    }

    for (uint64_t i = 0; i < 100; ++i) {
        r = BasicHistogram__insert_finite(&hist, random_values_0_to_11[i]);
        if (!r) {
            return false;
        }
    }
    for (uint64_t i = 0; i < 3; ++i) {
        r = BasicHistogram__insert_infinite(&hist);
        if (!r) {
            return false;
        }
    }
    // NOTE Histogram oracle generated by getting the random values generated
    //      above and running:
    //      y = [x.count(i) for i in range(10)]; print(y); z = x.count(10);
    //      print(z)
    // NOTE This makes no sense in the context of MRC generation since
    //      the number of infinities must equal the the number of unique
    //      elements used. However, this is not an MRC test, so it's OK!
    const uint64_t histogram_oracle[] = {9, 9, 12, 9, 4, 8, 15, 9, 6, 8};
    const uint64_t length_oracle = 10;
    const uint64_t false_infinity_oracle = 11;
    const uint64_t infinity_oracle = 3;
    const uint64_t running_sum_oracle = 103;

    for (uint64_t i = 0; i < 10; ++i) {
        ASSERT_TRUE_OR_RETURN_FALSE(hist.histogram[i] == histogram_oracle[i],
                                    "histogram should match oracle",
                                    &hist);
    }
    ASSERT_TRUE_OR_RETURN_FALSE(hist.length == length_oracle,
                                "length should match oracle",
                                &hist);
    ASSERT_TRUE_OR_RETURN_FALSE(hist.false_infinity == false_infinity_oracle,
                                "false_infinity should match oracle",
                                &hist);
    ASSERT_TRUE_OR_RETURN_FALSE(hist.infinity == infinity_oracle,
                                "infinity should match oracle",
                                &hist);
    ASSERT_TRUE_OR_RETURN_FALSE(hist.running_sum == running_sum_oracle,
                                "running_sum should match oracle",
                                &hist);
    return true;
}

static bool
test_fractional_histogram(void)
{
    struct FractionalHistogram me = {0};
    FractionalHistogram__init(&me, 100, 10);

    FractionalHistogram__insert_scaled_finite(&me, 25, 10, 1);
    FractionalHistogram__print_as_json(&me);
    FractionalHistogram__insert_scaled_finite(&me, 45, 10, 1);
    FractionalHistogram__print_as_json(&me);
    FractionalHistogram__insert_scaled_finite(&me, 55, 20, 1);
    FractionalHistogram__print_as_json(&me);
    FractionalHistogram__insert_scaled_finite(&me, 65, 10, 1);
    FractionalHistogram__print_as_json(&me);
    FractionalHistogram__insert_scaled_finite(&me, 75, 10, 1);
    FractionalHistogram__print_as_json(&me);
    FractionalHistogram__insert_scaled_finite(&me, 90, 100, 1);
    FractionalHistogram__insert_scaled_infinite(&me, 10);
    FractionalHistogram__print_as_json(&me);

    FractionalHistogram__destroy(&me);
    return true;
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    ASSERT_FUNCTION_RETURNS_TRUE(test_basic_histogram());
    ASSERT_FUNCTION_RETURNS_TRUE(test_fractional_histogram());
    return 0;
}
