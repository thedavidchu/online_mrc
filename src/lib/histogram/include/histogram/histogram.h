#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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

    // When we get a finite element larger than the current maximum,
    // double the range of our histogram by merging neighbouring buckets
    // until we can fit the element!
    bool allow_merging;
};

bool
Histogram__init(struct Histogram *me,
                size_t const num_bins,
                size_t const bin_size,
                bool const allow_merging);

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
Histogram__write_as_json(FILE *stream, struct Histogram *me);

/// @brief  Read the full histogram from a file.
bool
Histogram__init_from_file(struct Histogram *const me, char const *const path);

/// @brief  Save the full histogram to a file.
bool
Histogram__save_to_file(struct Histogram const *const me,
                        char const *const path);

/// @brief  Save the histogram in a sparse format of <index, frequency>.
bool
Histogram__save_sparse(struct Histogram const *const me,
                       char const *const path);

/// @brief  Write the Histogram as a JSON object to stdout.
void
Histogram__print_as_json(struct Histogram *me);

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

void
Histogram__destroy(struct Histogram *me);
