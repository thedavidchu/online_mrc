#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static char const *const HISTOGRAM_MODE_STRINGS[] = {
    "allow_overflow",
    "merge_bins",
    "realloc",
    "INVALID",
};

/// @brief  When we have an element that doesn't fit in the histogram,
///         we have multiple options of resolution.
///         1. Allow overflow (and record this as a 'false infinity')
///             This reduces the precision of overflowed values.
///         2. Merge the buckets to increase the bin size
///             This reduces the precision of all values.
///         3. Reallocate the buffer so that it is larger
///             (N.B. we must zero out the newly allocated space!)
///             This maintains the precision at the expense of larger
///             storage overheads.
enum HistogramOutOfBoundsMode {
    // NOTE This value is set to zero for backwards compatibility (with
    //      'false'). This guarantee is deprecated and future code
    //      should not rely on this.
    HistogramOutOfBoundsMode__allow_overflow,
    // When we get a finite element larger than the current maximum,
    // double the range of our histogram by merging neighbouring buckets
    // until we can fit the element!
    HistogramOutOfBoundsMode__merge_bins,
    HistogramOutOfBoundsMode__realloc,
    HistogramOutOfBoundsMode__INVALID,
};

bool
HistogramOutOfBoundsMode__parse(enum HistogramOutOfBoundsMode *me,
                                char const *const str);

/// @brief  Track (potentially scaled) equal-sized values by frequency.
/// @note   I assume no overflow in any of these values!
struct Histogram {
    uint64_t *histogram;
    /// Number of bins in the histogram
    size_t num_bins;
    // Size of each bin
    size_t bin_size;
    /// We have seen this before, but we do not track stacks this large
    uint64_t false_infinity;
    /// We have not seen this before
    uint64_t infinity;
    uint64_t running_sum;

    enum HistogramOutOfBoundsMode out_of_bounds_mode;
};

bool
Histogram__init(struct Histogram *me,
                size_t const num_bins,
                size_t const bin_size,
                enum HistogramOutOfBoundsMode const out_of_bounds_mode);

void
Histogram__destroy(struct Histogram *me);

bool
Histogram__insert_finite(struct Histogram *me, const uint64_t index);

/// @brief  Insert a non-infinite, scaled index. By scaled, I mean that the
///         index represents multiple elements.
/// @note   This is used for SHARDS.
bool
Histogram__insert_scaled_finite(struct Histogram *me,
                                const uint64_t index,
                                const uint64_t scale);

bool
Histogram__insert_infinite(struct Histogram *me);

bool
Histogram__insert_scaled_infinite(struct Histogram *me, const uint64_t scale);

uint64_t
Histogram__calculate_running_sum(struct Histogram *me);

bool
Histogram__exactly_equal(struct Histogram *me, struct Histogram *other);

bool
Histogram__debug_difference(struct Histogram *me,
                            struct Histogram *other,
                            const size_t max_num_mismatch);

void
Histogram__clear(struct Histogram *const me);

/// @brief  Write the Histogram as a JSON object to an arbitrary stream.
void
Histogram__write_as_json(FILE *const stream, struct Histogram const *const me);

/// @brief  Read the full histogram from a file.
bool
Histogram__load(struct Histogram *const me, char const *const path);

/// @brief  Save the full histogram to a file.
bool
Histogram__save(struct Histogram const *const me, char const *const path);

/// @brief  Write the Histogram as a JSON object to stdout.
void
Histogram__print_as_json(struct Histogram const *const me);

/// @brief  Adjust the histogram starting from the first bucket.
/// @note   This is for the SHARDS-Adj algorithm.
bool
Histogram__adjust_first_buckets(struct Histogram *me, int64_t const adjustment);

bool
Histogram__validate(struct Histogram const *const me);

double
Histogram__euclidean_error(struct Histogram const *const lhs,
                           struct Histogram const *const rhs);

/// @brief  Add 'other' histogram into 'me'.
/// @note   Python uses '__iadd__' to service the '+=' operator. That's
///         how I arrived at this somewhat cryptic name.
/// @todo   Add error checking.
bool
Histogram__iadd(struct Histogram *const me,
                struct Histogram const *const other);
