#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "arrays/array_size.h"
#include "histogram/histogram.h"
#include "invariants/implies.h"
#include "io/io.h"
#include "logger/logger.h"
#include "math/positive_ceiling_divide.h"

bool
HistogramOutOfBoundsMode__parse(enum HistogramOutOfBoundsMode *me,
                                char const *const str)
{
    for (size_t i = 0; i < ARRAY_SIZE(HISTOGRAM_MODE_STRINGS); ++i) {
        if (strcmp(HISTOGRAM_MODE_STRINGS[i], str) == 0) {
            *me = (enum HistogramOutOfBoundsMode)i;
            return true;
        }
    }
    return false;
}

static bool
init_histogram(struct Histogram *const me,
               size_t const num_bins,
               size_t const bin_size,
               uint64_t const false_infinity,
               uint64_t const infinity,
               uint64_t const running_sum,
               enum HistogramOutOfBoundsMode const out_of_bounds_mode)
{
    assert(me != NULL);

    // Assume it is either NULL or an uninitialized address!
    me->histogram = calloc(num_bins, sizeof(*me->histogram));
    if (me->histogram == NULL) {
        return false;
    }
    me->num_bins = num_bins;
    me->bin_size = bin_size;
    me->false_infinity = false_infinity;
    me->infinity = infinity;
    me->running_sum = running_sum;
    me->out_of_bounds_mode = out_of_bounds_mode;

    return true;
}

bool
Histogram__init(struct Histogram *me,
                size_t const num_bins,
                size_t const bin_size,
                enum HistogramOutOfBoundsMode const out_of_bounds_mode)
{
    if (me == NULL || num_bins == 0) {
        return false;
    }
    if (!init_histogram(me, num_bins, bin_size, 0, 0, 0, out_of_bounds_mode)) {
        LOGGER_ERROR("failed to init histogram");
        return false;
    }
    return true;
}

/// @brief  Check whether a histogram is properly initialized.
/// @note   This function disallows any of the size fields to be zero.
static bool
is_initialized(struct Histogram const *const me)
{
    if (me == NULL) {
        LOGGER_ERROR("Histogram is NULL");
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
    //      Recall `me->num_bins > 0` => `me->histogram != NULL`
    if (me->histogram == NULL) {
        LOGGER_ERROR("array of frequencies is NULL");
        return false;
    }
    // NOTE I hope the compiler optimizes this into a range comparison
    //      or something actually intelligent.
    if (me->out_of_bounds_mode != HistogramOutOfBoundsMode__allow_overflow &&
        me->out_of_bounds_mode != HistogramOutOfBoundsMode__merge_bins &&
        me->out_of_bounds_mode != HistogramOutOfBoundsMode__realloc) {
        LOGGER_ERROR("invalid out of bounds mode!");
        return false;
    }
    return true;
}

/// @brief  Double the size of each buckets to increase the histogram
///         range.
/// @note   I stole the logic from Ashvin's QuickMRC histogram
///         implementation.
static void
double_bin_size(struct Histogram *const me)
{
    assert(me);
    for (size_t i = 0; i < me->num_bins; i += 2) {
        me->histogram[i / 2] = me->histogram[i] + me->histogram[i + 1];
    }
    // NOTE This could be done more efficiently with a 'memset' but I am
    //      officially lazy (i.e. smart) and just copied Ashvin's
    //      implementation from his QuickMRC repo.
    for (size_t i = me->num_bins / 2; i < me->num_bins; ++i) {
        me->histogram[i] = 0;
    }
    me->bin_size = 2 * me->bin_size;
}

static bool
fits_in_histogram(struct Histogram const *const me,
                  uint64_t const index,
                  uint64_t const horizontal_scale)
{
    assert(me);
    uint64_t const scaled_index = horizontal_scale * index;
    return scaled_index < me->num_bins * me->bin_size;
}

static bool
merge_bins(struct Histogram *const me,
           uint64_t const index,
           uint64_t const horizontal_scale)
{
    assert(me);
    // NOTE This could be a do-while loop because in the only place we
    //      call this, we've already checked that the value doesn't fit
    //      in the histogram. I prefer to be explicit and clear.
    // NOTE I was thinking that in a well-behaved histogram, this
    //      should only happen once at a time. However, this isn't
    //      true. If we have a very tiny histogram array and access
    //      many new elements, then it will remain small until we
    //      suddenly access a very distantly used element.
    while (!fits_in_histogram(me, index, horizontal_scale)) {
        double_bin_size(me);
    }
    return true;
}

/// @param  alloc_amortization_factor: double
///         the factor by which to amortize the cost of reallocation.
static bool
alloc_more_histogram(struct Histogram *const me,
                     uint64_t const index,
                     uint64_t const horizontal_scale,
                     double const alloc_amortization_factor)
{
    assert(me && !fits_in_histogram(me, index, horizontal_scale));
    assert(alloc_amortization_factor >= 1.0 &&
           "cannot amortize by less than 1.0");

    // NOTE We must be able to accomodate a value of
    //      'index * horizontal_scale', which means we should add 1.
    size_t new_num_bins = alloc_amortization_factor *
                          (double)(index * horizontal_scale + 1) / me->bin_size;
    if (new_num_bins <= me->num_bins) {
        // NOTE This is extremely inefficient but we would only use it
        //      if larger allocations have failed.
        LOGGER_WARN("using inefficient reallocation strategy to go "
                    "from %zu to %zu bins",
                    me->num_bins,
                    new_num_bins);
        new_num_bins += me->num_bins + 1;
    }
    uint64_t *new_histogram =
        realloc(me->histogram, new_num_bins * sizeof(*me->histogram));
    // NOTE This error handling code is for large traces with many unique
    //      elements. Admittedly it is a bit confusing because we have
    //      variables being changed inside this if-statement.
    if (new_histogram == NULL) {
        // We need to provide a base case for the recursion.
        if (alloc_amortization_factor <= 1.0) {
            LOGGER_ERROR("unable to reallocate from %zu to %zu bins",
                         me->num_bins,
                         new_num_bins);
            return false;
        }
        LOGGER_WARN(
            "unable to reallocate %zu histogram bins. We'll try reducing the "
            "number of bins allocated (but we'll lose the amortized runtime)!",
            new_num_bins);
        // NOTE If we cannot allocate
        return alloc_more_histogram(me, index, horizontal_scale, 1.0);
    }
    // Zero the indices that have not been set.
    memset(&new_histogram[me->num_bins],
           0,
           (new_num_bins - me->num_bins) * sizeof(*new_histogram));
    me->histogram = new_histogram;
    me->num_bins = new_num_bins;
    return true;
}

/// @note   I use the term 'stretch' because I want it to encompass the
///         'allocate' and 'merge' operations. It isn't a great term.
static bool
stretch_histogram_if_necessary(struct Histogram *const me,
                               uint64_t const index,
                               uint64_t const horizontal_scale)
{
    // NOTE This should be the common case!
    if (fits_in_histogram(me, index, horizontal_scale)) {
        return true;
    }

    // NOTE This is a low entropy decision.
    switch (me->out_of_bounds_mode) {
    case HistogramOutOfBoundsMode__allow_overflow:
        return true;
    case HistogramOutOfBoundsMode__merge_bins:
        return merge_bins(me, index, horizontal_scale);
    case HistogramOutOfBoundsMode__realloc:
        return alloc_more_histogram(me, index, horizontal_scale, 1.5);
    default:
        return true;
    }
}

bool
Histogram__insert_finite(struct Histogram *me, const uint64_t index)
{
    return Histogram__insert_scaled_finite(me, index, 1);
}

bool
Histogram__insert_scaled_finite(struct Histogram *me,
                                const uint64_t index,
                                const uint64_t scale)
{
    if (!is_initialized(me)) {
        return false;
    }
    if (!stretch_histogram_if_necessary(me, index, scale)) {
        LOGGER_ERROR("stretch failed");
        return false;
    }
    // NOTE I think it's clearer to have more code in the if-blocks than to
    //      spread it around. The optimizing compiler should remove it.
    if (fits_in_histogram(me, index, scale)) {
        uint64_t const scaled_index = scale * index;
        me->histogram[scaled_index / me->bin_size] += scale;
        me->running_sum += scale;
    } else {
        me->false_infinity += scale;
        me->running_sum += scale;
    }
    return true;
}

bool
Histogram__insert_infinite(struct Histogram *me)
{
    return Histogram__insert_scaled_infinite(me, 1);
}

bool
Histogram__insert_scaled_infinite(struct Histogram *me, const uint64_t scale)
{
    if (!is_initialized(me)) {
        return false;
    }
    me->infinity += scale;
    me->running_sum += scale;
    return true;
}

uint64_t
Histogram__calculate_running_sum(struct Histogram *me)
{
    if (!is_initialized(me)) {
        return 0;
    }

    uint64_t running_sum = 0;
    for (size_t i = 0; i < me->num_bins; ++i) {
        running_sum += me->histogram[i];
    }
    running_sum += me->false_infinity + me->infinity;
    return running_sum;
}

void
Histogram__clear(struct Histogram *const me)
{
    if (!is_initialized(me)) {
        return;
    }

    memset(me->histogram, 0, me->num_bins * sizeof(*me->histogram));
    me->false_infinity = 0;
    me->infinity = 0;
    me->running_sum = 0;
}

void
Histogram__write_as_json(FILE *stream, struct Histogram *me)
{
    if (me == NULL) {
        fprintf(stream, "{\"type\": null}\n");
        return;
    }
    if (me->histogram == NULL) {
        fprintf(stream, "{\"type\": \"Histogram\", \".histogram\": null}\n");
        return;
    }
    fprintf(stream,
            "{\"type\": \"Histogram\", \".num_bins\": %" PRIu64
            ", \".bin_size\": %" PRIu64 ", \".running_sum\": %" PRIu64
            ", \".histogram\": {",
            me->num_bins,
            me->bin_size,
            me->running_sum);
    bool first_value = true;
    for (uint64_t i = 0; i < me->num_bins; ++i) {
        if (me->histogram[i] != 0) {
            if (first_value) {
                first_value = false;
            } else {
                fprintf(stream, ", ");
            }
            fprintf(stream,
                    "\"%" PRIu64 "\": %" PRIu64 "",
                    i * me->bin_size,
                    me->histogram[i]);
        }
    }
    // NOTE I assume me->num_bins is much less than SIZE_MAX
    fprintf(stream,
            "}, \".false_infinity\": %" PRIu64 ", \".infinity\": %" PRIu64
            "}\n",
            me->false_infinity,
            me->infinity);
}

void
Histogram__print_as_json(struct Histogram *me)
{
    Histogram__write_as_json(stdout, me);
}

bool
Histogram__exactly_equal(struct Histogram *me, struct Histogram *other)
{
    if (me == other) {
        LOGGER_DEBUG("Histograms are identical objects");
        return true;
    }
    if (me == NULL || other == NULL) {
        LOGGER_DEBUG("One Histograms is NULL");
        return false;
    }

    if (me->num_bins != other->num_bins || me->bin_size != other->bin_size) {
        LOGGER_DEBUG("Histograms differ in metadata (.num_bins: %zu vs %zu, "
                     ".bin_size: %zu vs %zu",
                     me->num_bins,
                     other->num_bins,
                     me->bin_size,
                     other->bin_size);
        return false;
    }
    if (me->false_infinity != other->false_infinity ||
        me->infinity != other->infinity ||
        me->running_sum != other->running_sum) {
        LOGGER_DEBUG("Histograms differ in non-histogram values ("
                     ".false_infinity: %zu vs %zu, .infinity: %zu vs %zu, "
                     ".running_sum: %zu vs %zu)",
                     me->false_infinity,
                     other->false_infinity,
                     me->infinity,
                     other->infinity,
                     me->running_sum,
                     other->running_sum);
        return false;
    }
    // NOTE I do not account for overflow; I believe overflow is impossible
    //      since the size of the address space * sizeof(uint64_t) < 2^64.
    if (memcmp(me->histogram,
               other->histogram,
               sizeof(*me->histogram) * me->num_bins) != 0) {
        return false;
    }
    return true;
}

bool
Histogram__debug_difference(struct Histogram *me,
                            struct Histogram *other,
                            const size_t max_num_mismatch)
{
    if (me == NULL || me->histogram == NULL || me->bin_size == 0 ||
        me->num_bins == 0) {
        LOGGER_DEBUG("Invalid me object");
        return false;
    }
    if (other == NULL || other->histogram == NULL || other->bin_size == 0 ||
        other->num_bins == 0) {
        LOGGER_DEBUG("Invalid other object");
        return false;
    }

    if (me->bin_size != other->bin_size || me->num_bins != other->num_bins) {
        LOGGER_DEBUG("Metadata mismatch: .bin_size = {%" PRIu64 ", %" PRIu64
                     "}, .num_bins = {%" PRIu64 ", %" PRIu64 "}",
                     me->bin_size,
                     other->bin_size,
                     me->num_bins,
                     other->num_bins);
        return false;
    }

    size_t num_mismatch = 0;
    for (size_t i = 0; i < me->num_bins; ++i) {
        if (me->histogram[i] != other->histogram[i]) {
            LOGGER_DEBUG("Mismatch at %zu: %" PRIu64 " vs %" PRIu64,
                         i,
                         me->histogram[i],
                         other->histogram[i]);
            ++num_mismatch;
        }

        if (num_mismatch >= max_num_mismatch) {
            LOGGER_DEBUG("too many mismatches!");
            return false;
        }
    }
    return num_mismatch == 0;
}

bool
Histogram__adjust_first_buckets(struct Histogram *me, int64_t const adjustment)
{
    if (!is_initialized(me))
        return false;

    // NOTE SHARDS-Adj only adds to the first bucket; but what if the
    //      adjustment would make it negative? Well, in that case, I
    //      add it to the next buckets. I figure this is OKAY because
    //      histogram bin size is configurable and it's like using a
    //      larger bin.
    int64_t tmp_adj = adjustment;
    for (size_t i = 0; i < me->num_bins; ++i) {
        int64_t hist = me->histogram[i];
        if ((int64_t)me->histogram[i] + tmp_adj < 0) {
            me->histogram[i] = 0;
            tmp_adj += hist;
        } else {
            me->histogram[i] += tmp_adj;
            tmp_adj = 0;
            break;
        }
    }

    me->running_sum += adjustment - tmp_adj;

    // If the adjustment is larger than the number of elements, then
    // we have a problem!
    if (tmp_adj != 0) {
        // If I print warnings in many places, then I effectively get a
        // stack trace! Isn't that nice?
        LOGGER_WARN("the attempted adjustment (%" PRId64 ") "
                    "is larger than the adjustment we managed (%" PRId64 ")!",
                    adjustment,
                    adjustment - tmp_adj);
        return false;
    }

    return true;
}

/// @brief  Write the metadata required to recreate the histogram.
/// @note   This must follow the same conventions as 'read_metadata'.
static bool
write_metadata(FILE *fp, struct Histogram const *const me)
{
    unsigned long r = 0;
    assert(fp != NULL && me != NULL);

    r = fwrite(&me->num_bins, sizeof(me->num_bins), 1, fp);
    assert(r == 1);
    r = fwrite(&me->bin_size, sizeof(me->bin_size), 1, fp);
    assert(r == 1);
    r = fwrite(&me->false_infinity, sizeof(me->false_infinity), 1, fp);
    assert(r == 1);
    r = fwrite(&me->infinity, sizeof(me->infinity), 1, fp);
    assert(r == 1);
    r = fwrite(&me->running_sum, sizeof(me->running_sum), 1, fp);
    assert(r == 1);

    return true;
}

/// @brief  Read the metadata required to recreate the histogram.
/// @note   This must follow the same conventions as 'write_metadata'.
static bool
read_metadata(FILE *fp, struct Histogram *const me)
{
    size_t r = 0;
    assert(fp != NULL && me != NULL);

    r = fread(&me->num_bins, sizeof(me->num_bins), 1, fp);
    assert(r == 1);
    r = fread(&me->bin_size, sizeof(me->bin_size), 1, fp);
    assert(r == 1);
    r = fread(&me->false_infinity, sizeof(me->false_infinity), 1, fp);
    assert(r == 1);
    r = fread(&me->infinity, sizeof(me->infinity), 1, fp);
    assert(r == 1);
    r = fread(&me->running_sum, sizeof(me->running_sum), 1, fp);
    assert(r == 1);

    return true;
}

static bool
write_index_miss_rate_pair(FILE *fp,
                           const uint64_t index,
                           const uint64_t bin_size,
                           const uint64_t frequency)
{
    size_t n = 0;
    uint64_t scaled_idx = index * bin_size;

    // We want to make sure we're writing the expected sizes out the file.
    // Otherwise, our reader will be confused. We should write out:
    // (uint64, float64)
    assert(sizeof(index) == 8 && sizeof(frequency) == 8 && "unexpected sizes");

    n = fwrite(&scaled_idx, sizeof(scaled_idx), 1, fp);
    if (n != 1) {
        LOGGER_ERROR("failed to write scaled index %zu", scaled_idx);
        return false;
    }
    n = fwrite(&frequency, sizeof(frequency), 1, fp);
    if (n != 1) {
        LOGGER_ERROR("failed to write object %zu: %g", scaled_idx, frequency);
        return false;
    }
    return true;
}

static bool
write_sparse_histogram(FILE *fp, struct Histogram const *const me)
{
    assert(fp != NULL && me != NULL && me->histogram != NULL &&
           me->num_bins != 0 && me->bin_size != 0);
    // NOTE I am assuming the endianness of the writer and reader will
    //      be the same.
    for (size_t i = 0; i < me->num_bins; ++i) {
        if (me->histogram[i] == 0)
            continue;
        if (!write_index_miss_rate_pair(fp,
                                        i,
                                        me->bin_size,
                                        me->histogram[i])) {

            LOGGER_ERROR("failed to write histogram");
            return false;
        }
    }
    return true;
}

static bool
read_sparse_histogram(FILE *fp, struct Histogram *const me)
{
    assert(fp != NULL && me != NULL && me->histogram != NULL &&
           me->num_bins != 0 && me->bin_size != 0);
    // NOTE I am assuming the endianness of the writer and reader will
    //      be the same.
    while (true) {
        size_t r = 0;
        uint64_t index = 0;
        uint64_t frequency = 0;

        r = fread(&index, sizeof(index), 1, fp);
        // HACK This isn't very nice code, sorry.
        if (r != 1) {
            assert(r == 0);
            return true;
        }
        r = fread(&frequency, sizeof(frequency), 1, fp);
        assert(r);

        assert(index % me->bin_size == 0);
        assert(index / me->bin_size < me->num_bins);
        me->histogram[index / me->bin_size] = frequency;
    }
    return true;
}

bool
Histogram__save(struct Histogram const *const me, char const *const path)
{
    if (!is_initialized(me) || path == NULL) {
        return false;
    }

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        LOGGER_ERROR("could not open '%s'", path);
        return false;
    }
    // NOTE I am assuming the endianness of the writer and reader will
    //      be the same.
    if (!write_metadata(fp, me)) {
        LOGGER_ERROR("failed to write metadata");
        goto cleanup;
    }
    if (!write_sparse_histogram(fp, me)) {
        LOGGER_ERROR("failed to write histogram");
        goto cleanup;
    }
    // Try to clean up regardless of the outcome of the fwrite(...).
    if (fclose(fp) != 0) {
        LOGGER_ERROR("failed to cleanup");
        return false;
    }
    return true;

cleanup:
    fclose(fp);
    return false;
}

/// @brief  Read the full histogram from a file.
bool
Histogram__load(struct Histogram *const me, char const *const path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        LOGGER_ERROR("fopen failed to open '%s'", path);
        return false;
    }

    if (!read_metadata(fp, me)) {
        LOGGER_ERROR("failed to read metadata");
        goto cleanup;
    }
    if (!init_histogram(me,
                        me->num_bins,
                        me->bin_size,
                        me->false_infinity,
                        me->infinity,
                        me->running_sum,
                        false)) {
        LOGGER_ERROR("init failed");
        goto cleanup;
    }
    if (!read_sparse_histogram(fp, me)) {
        LOGGER_ERROR("failed to read histogram");
        goto cleanup;
    }

    if (fclose(fp) != 0) {
        LOGGER_ERROR("could not close '%s'", path);
        return false;
    }

    return true;
cleanup:
    fclose(fp);
    Histogram__destroy(me);
    return false;
}

bool
Histogram__validate(struct Histogram const *const me)
{
    if (me == NULL) {
        // I guess this is an error?
        LOGGER_WARN("passed invalid argument");
        return false;
    }
    if (me->histogram == NULL && me->num_bins != 0) {
        LOGGER_ERROR("corrupted histogram");
        return false;
    }
    if (me->num_bins == 0 || me->bin_size == 0) {
        LOGGER_INFO("OK but empty histogram");
        return true;
    }

    uint64_t sum = 0;
    for (size_t i = 0; i < me->num_bins; ++i) {
        sum += me->histogram[i];
    }
    sum += me->false_infinity;
    sum += me->infinity;

    if (sum != me->running_sum) {
        LOGGER_ERROR("incorrect sum %" PRIu64 " vs %" PRIu64,
                     sum,
                     me->running_sum);
        return false;
    }

    return true;
}

double
Histogram__euclidean_error(struct Histogram const *const lhs,
                           struct Histogram const *const rhs)
{
    if (lhs == NULL || rhs == NULL) {
        LOGGER_WARN("passed invalid argument");
        return INFINITY;
    }
    if (!implies(lhs->num_bins != 0, lhs->histogram != NULL) ||
        !implies(rhs->num_bins != 0, rhs->histogram != NULL)) {
        LOGGER_ERROR("corrupted histogram");
        return INFINITY;
    }
    if (lhs->bin_size == 0 || rhs->bin_size == 0) {
        LOGGER_ERROR("bin_size == 0 in histogram");
        return INFINITY;
    }
    if (lhs->num_bins == 0 || rhs->num_bins == 0) {
        LOGGER_WARN("empty histogram array");
    }

    double mse = 0.0;
    for (size_t i = 0; i < MIN(lhs->num_bins, rhs->num_bins); ++i) {
        double diff = (double)lhs->histogram[i] - rhs->histogram[i];
        mse += diff * diff;
    }
    // For the histogram, after the end of shorter histogram, we assume
    // the shorter histogram's frequency values would have been zero.
    for (size_t i = MIN(lhs->num_bins, rhs->num_bins);
         i < MAX(lhs->num_bins, rhs->num_bins);
         ++i) {
        double diff = lhs->num_bins > rhs->num_bins ? lhs->histogram[i]
                                                    : rhs->histogram[i];
        mse += diff * diff;
    }
    double diff = (double)lhs->false_infinity - rhs->false_infinity;
    mse += diff * diff;
    diff = (double)lhs->infinity - rhs->infinity;
    mse += diff * diff;
    return sqrt(mse);
}

bool
Histogram__iadd(struct Histogram *const me, struct Histogram const *const other)
{
    assert(me != NULL);
    assert(other != NULL);
    assert(me->histogram != NULL);
    assert(other->histogram != NULL);
    assert(me->bin_size == other->bin_size);
    assert(me->num_bins == other->num_bins);

    for (size_t i = 0; i < me->num_bins; ++i) {
        me->histogram[i] += other->histogram[i];
    }
    me->false_infinity += other->false_infinity;
    me->infinity += other->infinity;

    return true;
}

void
Histogram__destroy(struct Histogram *me)
{
    if (me == NULL) {
        return;
    }
    free(me->histogram);
    *me = (struct Histogram){0};
    return;
}
