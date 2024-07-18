#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
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

// This is the number of bytes that the metadata takes up at the
// beginning of the file.
static size_t const METADATA_SIZE =
    sizeof(((struct MissRateCurve *)NULL)->num_bins) +
    sizeof(((struct MissRateCurve *)NULL)->bin_size);

/// @brief  Check whether an array is properly initialized.
/// @note   This function disallows any of the size fields to be zero.
static bool
is_initialized(struct MissRateCurve const *const me)
{
    if (me == NULL) {
        LOGGER_ERROR("MissRateCurve is NULL");
        return false;
    }
    if (me->num_bins == 0) {
        LOGGER_ERROR("number of bins is 0");
        return false;
    }
    if (me->bin_size == 0) {
        LOGGER_ERROR("bin size is 0");
        return false;
    }
    // NOTE We assert that the 'num_bins' is positive, so we cannot have
    //      a NULL miss rate array.
    //      Recall `me->num_bins > 0` => `me->miss_rate != NULL`
    if (me->miss_rate == NULL) {
        LOGGER_ERROR("array of miss-rates is NULL");
        return false;
    }
    return true;
}

bool
MissRateCurve__alloc_empty(struct MissRateCurve *const me,
                           uint64_t const num_mrc_bins,
                           uint64_t const bin_size)
{
    if (me == NULL || num_mrc_bins == 0 || bin_size == 0) {
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

struct SparseEntry {
    uint64_t key;
    double value;
};

/// @return     Returns {.key=UINT64_MAX, .value=0} upon out-of-bounds access.
///             We use this to our advantage below.
static struct SparseEntry
read_sparse_entry(struct MemoryMap const *const mm, size_t index)
{
    struct SparseEntry r = {0};
    if (METADATA_SIZE + index * sizeof(r) < mm->num_bytes) {
        assert(METADATA_SIZE + (index + 1) * sizeof(r) <= mm->num_bytes);
        uint8_t *tmp =
            &((uint8_t *)mm->buffer)[METADATA_SIZE + index * sizeof(r)];
        memcpy(&r, tmp, sizeof(r));
        return r;
    }
    return (struct SparseEntry){.key = UINT64_MAX, .value = 0.0};
}

static bool
write_sparse_entry(FILE *fp,
                   const uint64_t index,
                   const uint64_t bin_size,
                   const double miss_rate)
{
    uint64_t const scaled_idx = index * bin_size;
    struct SparseEntry const entry = {.key = scaled_idx, .value = miss_rate};
    if (fwrite(&entry, sizeof(entry), 1, fp) != 1) {
        LOGGER_ERROR("failed to write entry");
        return false;
    }
    return true;
}

bool
MissRateCurve__save(struct MissRateCurve const *const me,
                    char const *restrict const file_name)
{
    if (!is_initialized(me) || file_name == NULL) {
        return false;
    }

    FILE *fp = fopen(file_name, "wb");
    // NOTE I am assuming the endianness of the writer and reader will be
    // the same.
    if (fwrite(&me->num_bins, sizeof(me->num_bins), 1, fp) != 1) {
        LOGGER_ERROR("failed to write num_bins");
        goto cleanup;
    }
    if (fwrite(&me->bin_size, sizeof(me->bin_size), 1, fp) != 1) {
        LOGGER_ERROR("failed to write bin_size");
        goto cleanup;
    }
    // NOTE I do the 0th element separately so that from 1 onward, I can
    // simply
    //      compare with the previous.
    if (!write_sparse_entry(fp, 0, me->bin_size, me->miss_rate[0]))
        goto cleanup;
    for (size_t i = 1; i < me->num_bins; ++i) {
        if (me->miss_rate[i] == me->miss_rate[i - 1]) {
            continue;
        }
        if (!write_sparse_entry(fp, i, me->bin_size, me->miss_rate[i]))
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
MissRateCurve__load(struct MissRateCurve *const me,
                    char const *restrict const file_name)
{
    if (me == NULL || file_name == NULL) {
        return false;
    }

    // NOTE I ensure the cleanup works fine by setting the input data
    //      structure to {0}.
    *me = (struct MissRateCurve){0};

    struct MemoryMap mm = {0};
    if (!MemoryMap__init(&mm, file_name, "rb")) {
        LOGGER_ERROR("failed to open '%s'", file_name);
        goto cleanup;
    }

    // Read metadata.
    if (mm.num_bytes < METADATA_SIZE) {
        LOGGER_ERROR("not enough bytes to create num_bins and bin_size");
        goto cleanup;
    }
    size_t const num_bins = ((size_t *)mm.buffer)[0];
    size_t const bin_size = ((size_t *)mm.buffer)[1];

    // Ensure there is data to read.
    struct SparseEntry curr = {0}, next = {0};
    size_t num_entries = (mm.num_bytes - METADATA_SIZE) / sizeof(curr);
    if (num_entries == 0) {
        LOGGER_ERROR("not enough bytes to create an entry");
        goto cleanup;
    }
    if (num_entries > num_bins) {
        LOGGER_ERROR("too many entries considering the number of bins");
        goto cleanup;
    }

    // NOTE Yes, I know that I am setting all of the memory anyways,
    //      but 'calloc' is much easier to debug!
    me->miss_rate = (double *)calloc(num_bins, sizeof(*me->miss_rate));
    if (me->miss_rate == NULL) {
        LOGGER_ERROR("allocation failed!");
        goto cleanup;
    }
    size_t idx = 0;
    curr = read_sparse_entry(&mm, idx);
    next = read_sparse_entry(&mm, idx + 1);
    assert(curr.key == 0 && curr.value == 1.0);
    for (size_t i = 0; i < num_bins; ++i) {
        if (i == next.key) {
            ++idx;
            curr = read_sparse_entry(&mm, idx);
            next = read_sparse_entry(&mm, idx + 1);
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
cleanup:
    MemoryMap__destroy(&mm);
    free(me->miss_rate);
    return false;
}

bool
MissRateCurve__scaled_iadd(struct MissRateCurve *const me,
                           struct MissRateCurve const *const other,
                           double const scale)
{
    if (!is_initialized(me) || !is_initialized(other)) {
        return false;
    }

    if (me->num_bins != other->num_bins || me->bin_size != other->bin_size) {
        LOGGER_ERROR(
            "num_bins (%zu vs %zu) and bin_size (%zu vs %zu) must match",
            me->num_bins,
            other->num_bins,
            me->bin_size,
            other->bin_size);
        return false;
    }
    for (size_t i = 0; i < me->num_bins; ++i) {
        me->miss_rate[i] += scale * other->miss_rate[i];
    }
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
    if (!is_initialized(lhs)) {
        LOGGER_ERROR("invalid LHS");
        return false;
    }
    if (!is_initialized(rhs)) {
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

static size_t
get_working_set_size(struct MissRateCurve const *const me)
{
    if (!is_initialized(me)) {
        return 0;
    }
    size_t max_cache_size = 0;
    // We start from 1 just because it simplifies the logic. I believe
    // this is correct as it is... well, duh! Otherwise I wouldn't have
    // written it this way!
    for (size_t i = 1; i < me->num_bins; ++i) {
        // NOTE Miss rate is monotonically decreasing for LRU...
        assert(me->miss_rate[i] <= me->miss_rate[i - 1]);
        if (me->miss_rate[i] < me->miss_rate[i - 1]) {
            max_cache_size = i;
        }
    }
    assert(max_cache_size < me->num_bins);
    return max_cache_size;
}

static inline double
absolute_error(double const a, double const b)
{
    return ABS(a - b);
}

static inline double
squared_error(double const a, double const b)
{
    return (a - b) * (a - b);
}

/// @brief  Calculate the mean of some comparison function.
/// @note   I hope the C compiler inlines the function pointer!
static inline double
compute_mean_of_comparison(struct MissRateCurve const *const lhs,
                           struct MissRateCurve const *const rhs,
                           double (*compare)(double const, double const))
{
    // NOTE This condition holds when lhs == rhs == NULL and when they
    //      are a non-NULL value as well!
    if (lhs == rhs) {
        return 0.0;
    }
    if (!is_initialized(lhs) || !is_initialized(rhs)) {
        return INFINITY;
    }
    if (lhs->bin_size == 0 || rhs->bin_size == 0 ||
        lhs->bin_size != rhs->bin_size) {
        LOGGER_ERROR("cannot compare MRCs with different (or zero) bin sizes "
                     "(%zu vs %zu)",
                     lhs->bin_size,
                     rhs->bin_size);
        return INFINITY;
    }

    size_t const lhs_wss = get_working_set_size(lhs);
    size_t const rhs_wss = get_working_set_size(rhs);
    // NOTE I rely on the optimizing compiler to recognize that these
    //      if statements are logically equivalent. However, when I have
    //      it this way, I can const-qualify everything. Yay!
    size_t const min_wss = MIN(lhs_wss, rhs_wss);
    size_t const max_wss = MAX(lhs_wss, rhs_wss);
    // NOTE I use the '<' operator because that's what GLib's 'MIN'
    //      function uses and I wanted to use the identical operator so
    //      the compiler can best see this (it doesn't really matter).
    struct MissRateCurve const *const min_wss_mrc =
        lhs_wss < rhs_wss ? lhs : rhs;
    struct MissRateCurve const *const max_wss_mrc =
        lhs_wss < rhs_wss ? rhs : lhs;
    assert(min_wss_mrc != max_wss_mrc &&
           "my logic is incorrect above or we passed in two of the same "
           "MRCs and my handling of that case was deleted");
    assert(min_wss < min_wss_mrc->num_bins);
    assert(max_wss < max_wss_mrc->num_bins);

    // NOTE This may elucidate some of the boundary checking logic:
    //      0. WSS(min_wss_mrc) <= WSS(max_wss_mrc) [by definition]
    //      1. WSS(min_wss_mrc) <= |min_wss_mrc|
    //      2. WSS(max_wss_mrc) <= |max_wss_mrc|
    //      But there is no bound on the sizes of either MRCs and the
    //      size of min_wss_mrc versus WSS(max_wss_mrc).
    double comparison_sum = 0.0;
    // NOTE I can safely use the '<=' operator because I have checked
    //      that the WSS is strictly less than the size of each buffer.
    for (uint64_t i = 0; i <= min_wss; ++i) {
        comparison_sum += compare(lhs->miss_rate[i], rhs->miss_rate[i]);
    }
    double const mr_at_min_wss_of_min_wss_mrc = min_wss_mrc->miss_rate[min_wss];
    for (uint64_t i = min_wss; i <= max_wss; ++i) {
        comparison_sum +=
            compare(mr_at_min_wss_of_min_wss_mrc, max_wss_mrc->miss_rate[i]);
    }
    return comparison_sum / (double)(max_wss == 0 ? 1 : max_wss);
}

double
MissRateCurve__mean_absolute_error(struct MissRateCurve const *const lhs,
                                   struct MissRateCurve const *const rhs)
{
    return compute_mean_of_comparison(lhs, rhs, absolute_error);
}

double
MissRateCurve__mean_squared_error(struct MissRateCurve const *const lhs,
                                  struct MissRateCurve const *const rhs)
{
    return compute_mean_of_comparison(lhs, rhs, squared_error);
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
