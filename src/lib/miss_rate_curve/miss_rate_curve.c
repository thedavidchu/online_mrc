#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h> // for MIN()
#include <string.h>
#include <sys/types.h>

#include "histogram/fractional_histogram.h"
#include "io/io.h"
#include "logger/logger.h"
#include "math/doubles_are_equal.h"
#include "miss_rate_curve/miss_rate_curve.h"

bool
MissRateCurve__alloc_empty(struct MissRateCurve *const me,
                           uint64_t const num_mrc_bins,
                           uint64_t const bin_size)
{
    if (me == NULL) {
        return false;
    }
    *me = (struct MissRateCurve){
        .miss_rate = calloc(num_mrc_bins, sizeof(*me->miss_rate)),
        .num_bins = num_mrc_bins,
        .bin_size = bin_size};
    if (me->miss_rate == NULL) {
        LOGGER_ERROR("calloc failed");
        MissRateCurve__destroy(me);
        return false;
    }
    return true;
}

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
                                   struct Histogram const *const histogram)
{
    if (me == NULL || histogram == NULL || histogram->histogram == NULL ||
        histogram->num_bins == 0 || histogram->bin_size == 0) {
        return false;
    }
    if (histogram->running_sum == 0) {
        LOGGER_WARN("empty histogram");
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
    if (me == NULL || file_name == NULL) {
        return false;
    }
    me->miss_rate = (double *)malloc(num_bins * sizeof(*me->miss_rate));
    if (me->miss_rate == NULL) {
        return false;
    }
    FILE *fp = fopen(file_name, "rb");
    if (fp == NULL) {
        free(me->miss_rate);
        return false;
    }
    // NOTE I am assuming the endianness of the writer and reader will be the
    // same.
    size_t n = fread(me->miss_rate, sizeof(*me->miss_rate), num_bins, fp);
    // Try to clean up regardless of the outcome of the fread(...).
    int r = fclose(fp);
    if (n != num_bins || r != 0) {
        free(me->miss_rate);
        return false;
    }
    me->num_bins = num_bins;
    me->bin_size = bin_size;
    return true;
}

struct SparseEntry {
    uint64_t key;
    double value;
};

/// @return     Returns {0} upon out-of-bounds access.
///             We use this to our advantage below.
static struct SparseEntry
read_sparse_entry(struct MemoryMap *mm, size_t offset)
{
    struct SparseEntry r = {0};
    size_t num_entries = mm->num_bytes / (sizeof(r.key) + sizeof(r.value));
    assert(mm && mm->buffer);
    if (offset >= num_entries)
        return r;
    assert(mm->num_bytes % (sizeof(r.key) + sizeof(r.value)) == 0 &&
           "unexpected format");

    uint8_t *tmp =
        &((uint8_t *)mm->buffer)[offset * (sizeof(r.key) + sizeof(r.value))];
    memcpy(&r.key, &tmp[0], sizeof(r.key));
    memcpy(&r.value, &tmp[sizeof(r.key)], sizeof(r.value));
    return r;
}

bool
MissRateCurve__init_from_sparse_file(struct MissRateCurve *me,
                                     char const *restrict const file_name,
                                     const uint64_t num_bins,
                                     const uint64_t bin_size)
{
    if (me == NULL || file_name == NULL) {
        return false;
    }

    struct MemoryMap mm = {0};
    if (!MemoryMap__init(&mm, file_name, "rb")) {
        LOGGER_ERROR("failed to open '%s'", file_name);
        return false;
    }

    struct SparseEntry curr = {0}, next = {0};
    size_t num_entries = mm.num_bytes / (sizeof(curr.key) + sizeof(curr.value));
    if (num_entries == 0) {
        MemoryMap__destroy(&mm);
        return false;
    }

    me->miss_rate = (double *)malloc(num_bins * sizeof(*me->miss_rate));
    if (me->miss_rate == NULL) {
        return false;
    }
    size_t offset = 0; // Index into the mmap buffer
    curr = read_sparse_entry(&mm, 0);
    assert(curr.key == 0 && curr.value == 1.0);
    next = read_sparse_entry(&mm, 1);
    g_assert_cmpuint(num_entries, <=, num_bins);
    for (size_t i = 0; i < num_bins; ++i) {
        if (offset < num_entries - 1 && i == next.key) {
            ++offset;
            next = read_sparse_entry(&mm, offset + 1);
            curr = read_sparse_entry(&mm, offset);
        }
        me->miss_rate[i] = curr.value;
    }
    if (!MemoryMap__destroy(&mm)) {
        LOGGER_ERROR("failed to close '%s'", file_name);
        free(me->miss_rate);
        return false;
    }
    me->num_bins = num_bins;
    me->bin_size = bin_size;
    return true;
}

bool
MissRateCurve__write_binary_to_file(struct MissRateCurve const *const me,
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

static bool
write_index_miss_rate_pair(FILE *fp,
                           const uint64_t index,
                           const uint64_t bin_size,
                           const double miss_rate)
{
    size_t n = 0;
    uint64_t scaled_idx = index * bin_size;

    // We want to make sure we're writing the expected sizes out the file.
    // Otherwise, our reader will be confused. We should write out:
    // (uint64, float64)
    assert(sizeof(index) == 8 && sizeof(miss_rate) == 8 && "unexpected sizes");

    n = fwrite(&scaled_idx, sizeof(scaled_idx), 1, fp);
    if (n != 1) {
        LOGGER_ERROR("failed to write scaled index %zu", scaled_idx);
        return false;
    }
    n = fwrite(&miss_rate, sizeof(miss_rate), 1, fp);
    if (n != 1) {
        LOGGER_ERROR("failed to write object %zu: %g", scaled_idx, miss_rate);
        return false;
    }
    return true;
}

bool
MissRateCurve__write_sparse_binary_to_file(struct MissRateCurve const *const me,
                                           char const *restrict const file_name)
{
    if (me == NULL || me->miss_rate == NULL || me->num_bins == 0) {
        return false;
    }

    FILE *fp = fopen(file_name, "wb");
    // NOTE I am assuming the endianness of the writer and reader will be the
    // same.
    // NOTE I do the 0th element separately so that from 1 onward, I can simply
    //      compare with the previous.
    if (!write_index_miss_rate_pair(fp, 0, me->bin_size, me->miss_rate[0]))
        goto cleanup;
    for (size_t i = 1; i < me->num_bins; ++i) {
        if (me->miss_rate[i] == me->miss_rate[i - 1]) {
            continue;
        }
        if (!write_index_miss_rate_pair(fp, i, me->bin_size, me->miss_rate[i]))
            goto cleanup;
    }
    // Try to clean up regardless of the outcome of the fwrite(...).
    if (fclose(fp) != 0) {
        return false;
    }
    return true;

cleanup:
    fclose(fp);
    return false;
}

bool
MissRateCurve__add_scaled_histogram(struct MissRateCurve *const me,
                                    struct Histogram const *const hist,
                                    double const scale)
{
    if (me == NULL || me->miss_rate == NULL) {
        return false;
    }
    if (hist == NULL || hist->histogram == NULL) {
        return false;
    }

    struct MissRateCurve my_mrc = {0};
    MissRateCurve__init_from_histogram(&my_mrc, hist);
    if (me->num_bins != my_mrc.num_bins || me->bin_size != my_mrc.bin_size) {
        LOGGER_ERROR("num_bins and bin_size must match");
        return false;
    }
    for (size_t i = 0; i < me->num_bins; ++i) {
        me->miss_rate[i] += scale * my_mrc.miss_rate[i];
    }
    MissRateCurve__destroy(&my_mrc);
    return true;
}

bool
MissRateCurve__all_close(struct MissRateCurve const *const lhs,
                         struct MissRateCurve const *const rhs,
                         double const epsilon)
{
    // Standard error checks. To be honest, I'm all over the place with
    // my error checks. Sometimes, num_bins can be 0, other times, I do
    // not allow it... I don't even know if I disallow a value of 0 in
    // the initialization function.
    if (lhs == NULL || lhs->miss_rate == NULL || lhs->num_bins == 0 ||
        lhs->bin_size == 0) {
        LOGGER_ERROR("invalid LHS");
        return false;
    }
    if (rhs == NULL || rhs->miss_rate == NULL || rhs->num_bins == 0 ||
        rhs->bin_size == 0) {
        LOGGER_ERROR("invalid RHS");
        return false;
    }
    // These create a much more complex case and it is not worth my time
    // to handle these corner cases, so I just log the error.
    if (lhs->num_bins != rhs->num_bins) {
        LOGGER_ERROR("num_bins should match");
        return false;
    }
    if (lhs->bin_size != rhs->bin_size) {
        LOGGER_ERROR("bin_size should match");
        return false;
    }
    // Finally, we can get to the main meat of the function. There is so
    // much boilerplate code!
    bool ok = true;
    size_t const num_bins = lhs->num_bins;
    for (size_t i = 0; i < num_bins; ++i) {
        if (!doubles_are_close(lhs->miss_rate[i], rhs->miss_rate[i], epsilon)) {
            LOGGER_WARN("mismatch at index %zu: %g vs %g",
                        i,
                        lhs->miss_rate[i],
                        rhs->miss_rate[i]);
            ok = false;
        }
    }

    return ok;
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
    if (lhs->bin_size == 0 || rhs->bin_size == 0 ||
        lhs->bin_size != rhs->bin_size) {
        LOGGER_ERROR("cannot compare MRCs with different bin sizes");
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
    if (lhs->bin_size == 0 || rhs->bin_size == 0 ||
        lhs->bin_size != rhs->bin_size) {
        LOGGER_ERROR("cannot compare MRCs with different bin sizes");
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

bool
MissRateCurve__validate(struct MissRateCurve *me)
{
    if (me == NULL) {
        // I guess this is an error?
        LOGGER_WARN("passed invalid argument");
        return false;
    }
    if (me->miss_rate == NULL && me->num_bins != 0) {
        LOGGER_ERROR("corrupted MRC");
        return false;
    }
    if (me->num_bins == 0 || me->bin_size == 0) {
        LOGGER_INFO("OK but empty MRC");
        return true;
    }

    if (me->miss_rate[0] != 1.0) {
        LOGGER_ERROR("MRC[0] == %g != 1.0", me->miss_rate[0]);
        return false;
    }

    // Test monotonically decreasing
    double prev = me->miss_rate[0];
    for (size_t i = 1; i < me->num_bins; ++i) {
        if (me->miss_rate[i] > prev) {
            LOGGER_ERROR(
                "not monotonically decreasing MRC[%zu] = %g, MRC[%zu] = %g",
                i - 1,
                prev,
                i,
                me->miss_rate[i]);
            return false;
        }
        prev = me->miss_rate[i];
    }

    return true;
}

void
MissRateCurve__write_as_json(FILE *stream, struct MissRateCurve const *const me)
{
    if (me == NULL) {
        fprintf(stream, "{\"type\": null}\n");
        return;
    }
    if (me->miss_rate == NULL) {
        assert(me->num_bins == 0);
        fprintf(stream,
                "{\"type\": \"BasicMissRateCurve\", \"num_bins\": 0, "
                "\"bin_size\": 0, \"miss_rate\": null}\n");
        return;
    }

    fprintf(stream,
            "{\"type\": \"BasicMissRateCurve\", \"num_bins\": %" PRIu64
            ", \"bin_size\": %" PRIu64 ", \"miss_rate\": [",
            me->num_bins,
            me->bin_size);
    for (uint64_t i = 0; i < me->num_bins; ++i) {
        fprintf(stream,
                "%lf%s",
                me->miss_rate[i],
                (i != me->num_bins - 1) ? ", " : "");
    }
    fprintf(stream, "]}\n");
}

void
MissRateCurve__print_as_json(struct MissRateCurve *me)
{
    MissRateCurve__write_as_json(stdout, me);
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
