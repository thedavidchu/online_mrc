#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h> // for MIN()

#include "histogram/fractional_histogram.h"
#include "logger/logger.h"
#include "math/doubles_are_equal.h"
#include "miss_rate_curve/basic_miss_rate_curve.h"

bool
basic_miss_rate_curve__init_from_fractional_histogram(
    struct BasicMissRateCurve *me,
    struct FractionalHistogram *histogram)
{
    if (me == NULL || histogram == NULL || histogram->histogram == NULL ||
        histogram->length == 0) {
        return false;
    }
    // NOTE We include 1 past the histogram length to record "false infinities",
    //      i.e. elements past the maximum length of the histogram.
    const uint64_t length = histogram->length + 2;
    me->miss_rate = (double *)malloc(length * sizeof(*me->miss_rate));
    if (me->miss_rate == NULL) {
        return false;
    }
    me->length = length;

    // Generate the MRC
    const double total = (double)histogram->running_sum;
    double tmp = (double)total;
    for (uint64_t i = 0; i < histogram->length; ++i) {
        const double h = histogram->histogram[i];
        me->miss_rate[i] = tmp / total;
        assert(tmp + DBL_EPSILON >= h &&
               "the subtraction should yield a non-negative result");
        tmp -= h;
    }
    me->miss_rate[histogram->length] = tmp / total;
    tmp -= histogram->false_infinity;
    me->miss_rate[histogram->length + 1] = tmp / total;
    // NOTE The values are farther than DBL_EPSILON away from each other, but
    //      that is a very small value. I supplied my own value based on
    //      printing the values for the mimir test and taking as many
    //      significant digits as I could see.
    if (!doubles_are_close(tmp / total,
                           (double)histogram->infinity / total,
                           0.00001)) {
        LOGGER_ERROR("mismatch in infinities");
    }
    return true;
}

bool
basic_miss_rate_curve__init_from_basic_histogram(
    struct BasicMissRateCurve *me,
    struct BasicHistogram *histogram)
{
    if (me == NULL || histogram == NULL || histogram->histogram == NULL ||
        histogram->length == 0) {
        return false;
    }
    // NOTE We include 1 past the histogram length to record "false infinities",
    //      i.e. elements past the maximum length of the histogram.
    const uint64_t length = histogram->length + 2;
    me->miss_rate = (double *)malloc(length * sizeof(*me->miss_rate));
    if (me->miss_rate == NULL) {
        return false;
    }
    me->length = length;

    // Generate the MRC
    const uint64_t total = histogram->running_sum;
    uint64_t tmp = total;
    for (uint64_t i = 0; i < histogram->length; ++i) {
        const uint64_t h = histogram->histogram[i];
        // TODO(dchu): Check for division by zero! How do we intelligently
        //              resolve this?
        me->miss_rate[i] = (double)tmp / (double)total;
        assert(tmp >= h &&
               "the subtraction should yield a non-negative result");
        tmp -= h;
    }
    me->miss_rate[histogram->length] = (double)tmp / (double)total;
    tmp -= histogram->false_infinity;
    me->miss_rate[histogram->length + 1] = (double)tmp / (double)total;
    assert(tmp - histogram->infinity == 0);
    return true;
}

bool
basic_miss_rate_curve__init_from_parda_histogram(struct BasicMissRateCurve *me,
                                                 uint64_t histogram_length,
                                                 unsigned int *histogram,
                                                 uint64_t histogram_total,
                                                 uint64_t false_infinities)
{
    if (me == NULL || histogram == NULL || histogram == NULL ||
        histogram_length == 0) {
        return false;
    }
    // NOTE We include 1 past the histogram length to record "infinities". The
    //      "false infinity" is already recorded in the
    const uint64_t length = histogram_length + 2;
    me->miss_rate = (double *)malloc(length * sizeof(*me->miss_rate));
    if (me->miss_rate == NULL) {
        return false;
    }
    me->length = length;

    // Generate the MRC
    uint64_t tmp = histogram_total;
    for (uint64_t i = 0; i < histogram_length; ++i) {
        const uint64_t h = histogram[i];
        me->miss_rate[i] = (double)tmp / (double)histogram_total;
        assert(tmp >= h &&
               "the subtraction should yield a non-negative result");
        tmp -= h;
    }
    me->miss_rate[histogram_length] = (double)tmp / (double)histogram_total;
    tmp -= false_infinities;
    me->miss_rate[histogram_length + 1] = (double)tmp / (double)histogram_total;
    return true;
}

bool
basic_miss_rate_curve__init_from_file(struct BasicMissRateCurve *me,
                                      char const *restrict const file_name,
                                      const uint64_t length)
{
    if (me == NULL) {
        return false;
    }
    me->miss_rate = (double *)malloc(length * sizeof(*me->miss_rate));
    if (me->miss_rate == NULL) {
        return false;
    }
    FILE *fp = fopen(file_name, "rb");
    // NOTE I am assuming the endianness of the writer and reader will be the
    // same.
    size_t n = fread(me->miss_rate, sizeof(*me->miss_rate), length, fp);
    // Try to clean up regardless of the outcome of the fread(...).
    int r = fclose(fp);
    if (n != length || r != 0) {
        return false;
    }
    me->length = length;
    return true;
}

bool
basic_miss_rate_curve__write_binary_to_file(
    struct BasicMissRateCurve *me,
    char const *restrict const file_name)
{
    if (me == NULL || me->miss_rate == NULL) {
        return false;
    }
    FILE *fp = fopen(file_name, "wb");
    // NOTE I am assuming the endianness of the writer and reader will be the
    // same.
    size_t n = fwrite(me->miss_rate, sizeof(*me->miss_rate), me->length, fp);
    // Try to clean up regardless of the outcome of the fwrite(...).
    int r = fclose(fp);
    if (n != me->length || r != 0) {
        return false;
    }
    return true;
}

double
basic_miss_rate_curve__mean_squared_error(struct BasicMissRateCurve *lhs,
                                          struct BasicMissRateCurve *rhs)
{
    if (lhs == NULL && rhs == NULL) {
        return 0.0;
    }
    // Correctness assertions
    if (lhs->miss_rate == NULL && lhs->length != 0) {
        return -1.0;
    }
    if (rhs->miss_rate == NULL && rhs->length != 0) {
        return -1.0;
    }

    const uint64_t min_bound = MIN(lhs->length, rhs->length);
    const uint64_t max_bound = MAX(lhs->length, rhs->length);
    double mse = 0.0;
    for (uint64_t i = 0; i < min_bound; ++i) {
        const double diff = lhs->miss_rate[i] - rhs->miss_rate[i];
        mse += diff * diff;
    }
    for (uint64_t i = min_bound; i < max_bound; ++i) {
        // NOTE I'm assuming the compiler pulls this if-statement out of the
        //      loop. I think this arrangement is more idiomatic than having
        //      separate for-loops.
        double diff = (lhs->length > rhs->length)
                          ? lhs->miss_rate[i] - rhs->miss_rate[min_bound - 1]
                          : rhs->miss_rate[i] - lhs->miss_rate[min_bound - 1];
        mse += diff * diff;
    }
    return mse / (double)(max_bound == 0 ? 1 : max_bound);
}

double
basic_miss_rate_curve__mean_absolute_error(struct BasicMissRateCurve *lhs,
                                           struct BasicMissRateCurve *rhs)
{
    if (lhs == NULL && rhs == NULL) {
        return 0.0;
    }
    // Correctness assertions
    if (lhs->miss_rate == NULL && lhs->length != 0) {
        return -1.0;
    }
    if (rhs->miss_rate == NULL && rhs->length != 0) {
        return -1.0;
    }

    const uint64_t min_bound = MIN(lhs->length, rhs->length);
    const uint64_t max_bound = MAX(lhs->length, rhs->length);
    double mae = 0.0;
    for (uint64_t i = 0; i < min_bound; ++i) {
        // NOTE This is just a little (potential) optimization to have the ABS
        //      act on `diff` alone because we do restrict pointer aliasing for
        //      `miss_rate`, so the compiler may force it to do repeated memory
        //      accesses. I'm not sure. Either way, I find this more readable.
        const double diff = lhs->miss_rate[i] - rhs->miss_rate[i];
        mae += ABS(diff);
    }
    for (uint64_t i = min_bound; i < max_bound; ++i) {
        // NOTE I'm assuming the compiler pulls this if-statement out of the
        //      loop. I think this arrangement is more idiomatic than having
        //      separate for-loops.
        double diff = (lhs->length > rhs->length)
                          ? lhs->miss_rate[i] - rhs->miss_rate[min_bound - 1]
                          : rhs->miss_rate[i] - lhs->miss_rate[min_bound - 1];
        mae += ABS(diff);
    }
    return mae / (double)(max_bound == 0 ? 1 : max_bound);
}

void
basic_miss_rate_curve__print_as_json(struct BasicMissRateCurve *me)
{
    if (me == NULL) {
        printf("{\"type\": null}\n");
        return;
    }
    if (me->miss_rate == NULL) {
        assert(me->length == 0);
        printf("{\"type\": \"BasicMissRateCurve\", \"length\": 0, "
               "\"miss_rate\": null}\n");
        return;
    }

    printf("{\"type\": \"BasicMissRateCurve\", \"length\": %" PRIu64
           ", \"miss_rate\": [",
           me->length);
    for (uint64_t i = 0; i < me->length; ++i) {
        printf("%lf%s", me->miss_rate[i], (i != me->length - 1) ? ", " : "");
    }
    printf("]}\n");
}

void
basic_miss_rate_curve__destroy(struct BasicMissRateCurve *me)
{
    if (me == NULL) {
        return;
    }
    free(me->miss_rate);
    *me = (struct BasicMissRateCurve){0};
}
