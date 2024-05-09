#include <assert.h>
#include <bits/stdint-uintn.h>
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
#include "miss_rate_curve/miss_rate_curve.h"

bool
MissRateCurve__init_from_fractional_histogram(
    struct MissRateCurve *me,
    struct FractionalHistogram *histogram)
{
    if (me == NULL || histogram == NULL || histogram->histogram == NULL ||
        histogram->num_bins == 0) {
        return false;
    }
    // NOTE We include 1 past the histogram length to record "false infinities",
    //      i.e. elements past the maximum length of the histogram.
    const uint64_t num_bins = histogram->num_bins + 2;
    me->miss_rate = (double *)malloc(num_bins * sizeof(*me->miss_rate));
    if (me->miss_rate == NULL) {
        return false;
    }
    me->num_bins = num_bins;
    me->bin_size = histogram->bin_size;

    // Generate the MRC
    const double total = (double)histogram->running_sum;
    double tmp = (double)total;
    for (uint64_t i = 0; i < histogram->num_bins; ++i) {
        const double h = histogram->histogram[i];
        me->miss_rate[i] = tmp / total;
        assert(tmp + DBL_EPSILON >= h &&
               "the subtraction should yield a non-negative result");
        tmp -= h;
    }
    me->miss_rate[histogram->num_bins] = tmp / total;
    tmp -= histogram->false_infinity;
    me->miss_rate[histogram->num_bins + 1] = tmp / total;
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
MissRateCurve__init_from_histogram(struct MissRateCurve *me,
                                   struct Histogram *histogram)
{
    if (me == NULL || histogram == NULL || histogram->histogram == NULL ||
        histogram->num_bins == 0 || histogram->bin_size == 0) {
        return false;
    }
    // NOTE We include 1 past the histogram length to record "false infinities",
    //      i.e. elements past the maximum length of the histogram.
    const uint64_t num_bins = histogram->num_bins + 2;
    me->miss_rate = (double *)malloc(num_bins * sizeof(*me->miss_rate));
    if (me->miss_rate == NULL) {
        return false;
    }
    me->num_bins = num_bins;
    me->bin_size = histogram->bin_size;

    // Generate the MRC
    const uint64_t total = histogram->running_sum;
    uint64_t tmp = total;
    for (uint64_t i = 0; i < histogram->num_bins; ++i) {
        const uint64_t h = histogram->histogram[i];
        // TODO(dchu): Check for division by zero! How do we intelligently
        //              resolve this?
        me->miss_rate[i] = (double)tmp / (double)total;
        // The subtraction should yield a non-negative result
        g_assert_cmpuint(tmp, >=, h);
        tmp -= h;
    }
    me->miss_rate[histogram->num_bins] = (double)tmp / (double)total;
    tmp -= histogram->false_infinity;
    me->miss_rate[histogram->num_bins + 1] = (double)tmp / (double)total;
    // We want to check that there is nothing left over!
    g_assert_cmpuint(tmp, ==, histogram->infinity);
    return true;
}

bool
MissRateCurve__init_from_parda_histogram(struct MissRateCurve *me,
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
    const uint64_t num_bins = histogram_length + 2;
    me->miss_rate = (double *)malloc(num_bins * sizeof(*me->miss_rate));
    if (me->miss_rate == NULL) {
        return false;
    }
    me->num_bins = num_bins;
    me->bin_size = 1;

    // Generate the MRC
    uint64_t tmp = histogram_total;
    for (uint64_t i = 0; i < histogram_length; ++i) {
        const uint64_t h = histogram[i];
        me->miss_rate[i] = (double)tmp / (double)histogram_total;
        // The subtraction should yield a non-negative result
        g_assert_cmpuint(tmp, >=, h);
        tmp -= h;
    }
    me->miss_rate[histogram_length] = (double)tmp / (double)histogram_total;
    tmp -= false_infinities;
    me->miss_rate[histogram_length + 1] = (double)tmp / (double)histogram_total;
    return true;
}

bool
MissRateCurve__init_from_file(struct MissRateCurve *me,
                              char const *restrict const file_name,
                              const uint64_t num_bins,
                              const uint64_t bin_size)
{
    if (me == NULL) {
        return false;
    }
    me->miss_rate = (double *)malloc(num_bins * sizeof(*me->miss_rate));
    if (me->miss_rate == NULL) {
        return false;
    }
    FILE *fp = fopen(file_name, "rb");
    // NOTE I am assuming the endianness of the writer and reader will be the
    // same.
    size_t n = fread(me->miss_rate, sizeof(*me->miss_rate), num_bins, fp);
    // Try to clean up regardless of the outcome of the fread(...).
    int r = fclose(fp);
    if (n != num_bins || r != 0) {
        return false;
    }
    me->num_bins = num_bins;
    me->bin_size = bin_size;
    return true;
}

bool
MissRateCurve__write_binary_to_file(struct MissRateCurve *me,
                                    char const *restrict const file_name)
{
    if (me == NULL || me->miss_rate == NULL) {
        return false;
    }
    FILE *fp = fopen(file_name, "wb");
    // NOTE I am assuming the endianness of the writer and reader will be the
    // same.
    size_t n = fwrite(me->miss_rate, sizeof(*me->miss_rate), me->num_bins, fp);
    // Try to clean up regardless of the outcome of the fwrite(...).
    int r = fclose(fp);
    if (n != me->num_bins || r != 0) {
        return false;
    }
    return true;
}

double
MissRateCurve__mean_squared_error(struct MissRateCurve *lhs,
                                  struct MissRateCurve *rhs)
{
    if (lhs == NULL && rhs == NULL) {
        return 0.0;
    }
    // Correctness assertions
    if (lhs->miss_rate == NULL && lhs->num_bins != 0) {
        return -1.0;
    }
    if (rhs->miss_rate == NULL && rhs->num_bins != 0) {
        return -1.0;
    }

    const uint64_t min_bound = MIN(lhs->num_bins, rhs->num_bins);
    const uint64_t max_bound = MAX(lhs->num_bins, rhs->num_bins);
    double mse = 0.0;
    for (uint64_t i = 0; i < min_bound; ++i) {
        const double diff = lhs->miss_rate[i] - rhs->miss_rate[i];
        mse += diff * diff;
    }
    for (uint64_t i = min_bound; i < max_bound; ++i) {
        // NOTE I'm assuming the compiler pulls this if-statement out of the
        //      loop. I think this arrangement is more idiomatic than having
        //      separate for-loops.
        double diff = (lhs->num_bins > rhs->num_bins)
                          ? lhs->miss_rate[i] - rhs->miss_rate[min_bound - 1]
                          : rhs->miss_rate[i] - lhs->miss_rate[min_bound - 1];
        mse += diff * diff;
    }
    return mse / (double)(max_bound == 0 ? 1 : max_bound);
}

double
MissRateCurve__mean_absolute_error(struct MissRateCurve *lhs,
                                   struct MissRateCurve *rhs)
{
    if (lhs == NULL && rhs == NULL) {
        return 0.0;
    }
    // Correctness assertions
    if (lhs->miss_rate == NULL && lhs->num_bins != 0) {
        return -1.0;
    }
    if (rhs->miss_rate == NULL && rhs->num_bins != 0) {
        return -1.0;
    }

    const uint64_t min_bound = MIN(lhs->num_bins, rhs->num_bins);
    const uint64_t max_bound = MAX(lhs->num_bins, rhs->num_bins);
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
        double diff = (lhs->num_bins > rhs->num_bins)
                          ? lhs->miss_rate[i] - rhs->miss_rate[min_bound - 1]
                          : rhs->miss_rate[i] - lhs->miss_rate[min_bound - 1];
        mae += ABS(diff);
    }
    return mae / (double)(max_bound == 0 ? 1 : max_bound);
}

void
MissRateCurve__print_as_json(struct MissRateCurve *me)
{
    if (me == NULL) {
        printf("{\"type\": null}\n");
        return;
    }
    if (me->miss_rate == NULL) {
        assert(me->num_bins == 0);
        printf("{\"type\": \"BasicMissRateCurve\", \"length\": 0, "
               "\"miss_rate\": null}\n");
        return;
    }

    printf("{\"type\": \"BasicMissRateCurve\", \"length\": %" PRIu64
           ", \"miss_rate\": [",
           me->num_bins);
    for (uint64_t i = 0; i < me->num_bins; ++i) {
        printf("%lf%s", me->miss_rate[i], (i != me->num_bins - 1) ? ", " : "");
    }
    printf("]}\n");
}

void
MissRateCurve__destroy(struct MissRateCurve *me)
{
    if (me == NULL) {
        return;
    }
    free(me->miss_rate);
    *me = (struct MissRateCurve){0};
}
