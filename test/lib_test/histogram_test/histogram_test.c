#include <bits/stdint-uintn.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "histogram/fractional_histogram.h"
#include "histogram/histogram.h"

#include "logger/logger.h"
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
            Histogram__destroy((histogram_ptr));                               \
            printf("[ERROR] %s:%d %s\n", __FILE__, __LINE__, (msg));           \
            /* NOTE This assertion is for debugging purposes so that we have a \
            finer grain understanding of where the failure occurred. */        \
            assert((r) && (msg));                                              \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool
test_histogram(void)
{
    struct Histogram hist = {0};

    bool r = Histogram__init(&hist, 10, 1, false);
    if (!r) {
        return false;
    }

    for (uint64_t i = 0; i < 100; ++i) {
        r = Histogram__insert_finite(&hist, random_values_0_to_11[i]);
        if (!r) {
            return false;
        }
    }
    for (uint64_t i = 0; i < 3; ++i) {
        r = Histogram__insert_infinite(&hist);
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
    // NOTE I removed the const from 'histogram_oracle' so that I could
    //      construct a struct Histogram from it without the compiler
    //      whining. C's support for const-ness is so rudimentary!
    uint64_t histogram_oracle[] = {9, 9, 12, 9, 4, 8, 15, 9, 6, 8};
    const uint64_t length_oracle = 10;
    const uint64_t false_infinity_oracle = 11;
    const uint64_t infinity_oracle = 3;
    const uint64_t running_sum_oracle = 103;

    for (uint64_t i = 0; i < 10; ++i) {
        ASSERT_TRUE_OR_RETURN_FALSE(hist.histogram[i] == histogram_oracle[i],
                                    "histogram should match oracle",
                                    &hist);
    }
    ASSERT_TRUE_OR_RETURN_FALSE(hist.num_bins == length_oracle,
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
    g_assert_cmpfloat(
        Histogram__euclidean_error(
            &hist,
            &(struct Histogram){.histogram = histogram_oracle,
                                .num_bins = length_oracle,
                                .bin_size = 1,
                                .false_infinity = false_infinity_oracle,
                                .infinity = infinity_oracle}),
        ==,
        0.0);
    return true;
}

static bool
test_binned_histogram(void)
{
    struct Histogram hist = {0};
    g_assert_true(Histogram__init(&hist, 10, 2, false));

    for (uint64_t i = 0; i < 100; ++i) {
        g_assert_true(
            Histogram__insert_finite(&hist, random_values_0_to_11[i]));
    }
    for (uint64_t i = 0; i < 4; ++i) {
        g_assert_true(Histogram__insert_finite(&hist, 20));
    }
    for (uint64_t i = 0; i < 3; ++i) {
        g_assert_true(Histogram__insert_infinite(&hist));
    }
    // NOTE Histogram oracle generated by getting the random values generated
    //      above and running:
    //      y = [x.count(i) for i in range(10)]; print(y); z = x.count(10);
    //      print(z)
    // NOTE This makes no sense in the context of MRC generation since
    //      the number of infinities must equal the the number of unique
    //      elements used. However, this is not an MRC test, so it's OK!
    const uint64_t histogram_oracle[10] =
        {9 + 9, 12 + 9, 4 + 8, 15 + 9, 6 + 8, 11};
    const uint64_t num_bins_oracle = 10;
    const uint64_t bin_size_oracle = 2;
    const uint64_t false_infinity_oracle = 4;
    const uint64_t infinity_oracle = 3;
    const uint64_t running_sum_oracle = 100 + 4 + 3;

    Histogram__print_as_json(&hist);

    for (uint64_t i = 0; i < 10; ++i) {
        g_assert_cmpuint(hist.histogram[i], ==, histogram_oracle[i]);
    }
    g_assert_cmpuint(hist.num_bins, ==, num_bins_oracle);
    g_assert_cmpuint(hist.bin_size, ==, bin_size_oracle);
    g_assert_cmpuint(hist.false_infinity, ==, false_infinity_oracle);
    g_assert_cmpuint(hist.infinity, ==, infinity_oracle);
    g_assert_cmpuint(hist.running_sum, ==, running_sum_oracle);
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

    g_assert_true(FractionalHistogram__validate(&me));
    // This is the scaled number of inserts (1+1+1+1+1+10)
    g_assert_true(me.running_sum == 16);

    FractionalHistogram__destroy(&me);
    return true;
}

static bool
test_histogram_save(void)
{
    uint64_t histogram[100] = {0};
    struct Histogram a = {
        .histogram = histogram,
        .num_bins = 100,
        .bin_size = 10,
        .false_infinity = 200,
        .infinity = 300,
        .running_sum = 400,
    };
    struct Histogram b = {0};

    memcpy(histogram, random_values_0_to_11, sizeof(random_values_0_to_11));
    bool r = false;
    r = Histogram__save_to_file(&a, "./histogram_test.bin");
    g_assert_true(r);
    r = Histogram__init_from_file(&b, "./histogram_test.bin");
    g_assert_true(r);

    // Cleanup files!
    g_assert_cmpint(remove("./histogram_test.bin"), ==, 0);

    g_assert_cmpuint(a.num_bins, ==, b.num_bins);
    g_assert_cmpuint(a.bin_size, ==, b.bin_size);
    g_assert_cmpuint(a.false_infinity, ==, b.false_infinity);
    g_assert_cmpuint(a.infinity, ==, b.infinity);
    g_assert_cmpuint(a.running_sum, ==, b.running_sum);

    for (size_t i = 0; i < a.num_bins; ++i) {
        g_assert_cmpuint(a.histogram[i], ==, b.histogram[i]);
    }

    Histogram__destroy(&b);
    return true;
}

static bool
test_histogram_with_false_infinity_on_outofbounds(void)
{
    struct Histogram me = {0};
    if (!Histogram__init(&me, 1, 1, HistogramOutOfBoundsMode__allow_overflow)) {
        LOGGER_ERROR("failed init");
        return false;
    }
    Histogram__insert_scaled_infinite(&me, 13);
    for (size_t i = 0; i < 100; ++i) {
        Histogram__insert_scaled_finite(&me, i, 7);
    }

    g_assert_cmpuint(me.num_bins, ==, 1);
    g_assert_cmpuint(me.bin_size, ==, 1);
    g_assert_cmpuint(me.infinity, ==, 13);
    g_assert_cmpuint(me.running_sum, ==, 13 + 7 * 100);
    g_assert_cmpuint(me.false_infinity, ==, 7 * 99);
    g_assert_cmpint(me.out_of_bounds_mode,
                    ==,
                    HistogramOutOfBoundsMode__allow_overflow);
    g_assert_cmpuint(me.histogram[0], ==, 7);
    return true;
}

static bool
test_histogram_with_merge_on_outofbounds(void)
{
    struct Histogram me = {0};
    if (!Histogram__init(&me, 10, 1, HistogramOutOfBoundsMode__merge_bins)) {
        LOGGER_ERROR("failed init");
        return false;
    }
    Histogram__insert_scaled_infinite(&me, 13);
    for (size_t i = 0; i < 100; ++i) {
        Histogram__insert_scaled_finite(&me, i, 10);
    }

    g_assert_cmpuint(me.num_bins, ==, 10);
    g_assert_cmpuint(me.bin_size, ==, 1 << 7);
    g_assert_cmpuint(me.infinity, ==, 13);
    g_assert_cmpuint(me.running_sum, ==, 13 + 10 * 100);
    g_assert_cmpuint(me.false_infinity, ==, 0);
    g_assert_cmpint(me.out_of_bounds_mode,
                    ==,
                    HistogramOutOfBoundsMode__merge_bins);
    // NOTE This weird spacing is because each bucket is 128 spaces wide but the
    //      multiplicative scale is base-10.
    g_assert_cmpuint(me.histogram[0], ==, 130);
    g_assert_cmpuint(me.histogram[1], ==, 130);
    g_assert_cmpuint(me.histogram[2], ==, 130);
    g_assert_cmpuint(me.histogram[3], ==, 130);
    g_assert_cmpuint(me.histogram[4], ==, 120);
    g_assert_cmpuint(me.histogram[5], ==, 130);
    g_assert_cmpuint(me.histogram[6], ==, 130);
    g_assert_cmpuint(me.histogram[7], ==, 100);
    g_assert_cmpuint(me.histogram[8], ==, 0);
    g_assert_cmpuint(me.histogram[9], ==, 0);
    return true;
}

static bool
test_histogram_with_realloc_on_outofbounds(void)
{
    struct Histogram me = {0};
    if (!Histogram__init(&me, 1, 3, HistogramOutOfBoundsMode__realloc)) {
        LOGGER_ERROR("failed init");
        return false;
    }
    Histogram__insert_scaled_infinite(&me, 13);
    for (size_t i = 0; i < 100; ++i) {
        Histogram__insert_scaled_finite(&me, i, 3);
    }

    g_assert_cmpuint(me.num_bins, ==, 100);
    g_assert_cmpuint(me.bin_size, ==, 3);
    g_assert_cmpuint(me.infinity, ==, 13);
    g_assert_cmpuint(me.running_sum, ==, 13 + 3 * 100);
    g_assert_cmpuint(me.false_infinity, ==, 0);
    g_assert_cmpint(me.out_of_bounds_mode,
                    ==,
                    HistogramOutOfBoundsMode__realloc);
    for (size_t i = 0; i < 100; ++i) {
        g_assert_cmpuint(me.histogram[i], ==, 3);
    }
    return true;
}

int
main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    ASSERT_FUNCTION_RETURNS_TRUE(test_histogram());
    ASSERT_FUNCTION_RETURNS_TRUE(test_binned_histogram());
    ASSERT_FUNCTION_RETURNS_TRUE(test_fractional_histogram());
    ASSERT_FUNCTION_RETURNS_TRUE(test_histogram_save());
    ASSERT_FUNCTION_RETURNS_TRUE(
        test_histogram_with_false_infinity_on_outofbounds());
    ASSERT_FUNCTION_RETURNS_TRUE(test_histogram_with_merge_on_outofbounds());
    ASSERT_FUNCTION_RETURNS_TRUE(test_histogram_with_realloc_on_outofbounds());
    return 0;
}
