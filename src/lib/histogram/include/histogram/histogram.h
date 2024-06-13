#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/// @brief  This histogram tracks (potentially scaled) equal-sized values.
/// @note   I assume no overflow in any of these values!
struct Histogram {
    uint64_t *histogram;
    /// Number of bins in the histogram
    uint64_t num_bins;
    // Size of each bin
    uint64_t bin_size;
    /// We have seen this before, but we do not track stacks this large
    uint64_t false_infinity;
    /// We have not seen this before
    uint64_t infinity;
    uint64_t running_sum;
};

bool
Histogram__init(struct Histogram *me,
                const uint64_t num_bins,
                const uint64_t bin_size);

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

bool
Histogram__exactly_equal(struct Histogram *me, struct Histogram *other);

bool
Histogram__debug_difference(struct Histogram *me,
                            struct Histogram *other,
                            const size_t max_num_mismatch);

/// @brief  Write the Histogram as a JSON object to an arbitrary stream.
void
Histogram__write_as_json(FILE *stream, struct Histogram *me);

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

void
Histogram__destroy(struct Histogram *me);
