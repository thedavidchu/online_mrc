#pragma once

#include <stdbool.h>
#include <stdint.h>

/// @brief  This histogram tracks (potentially scaled) equal-sized values.
/// @note   I assume no overflow in any of these values!
struct FractionalHistogram {
    double *histogram;
    /// Maximum allowable length of the histogram
    uint64_t length;
    /// We have seen this before, but we do not track stacks this large
    double false_infinity;
    /// We have not seen this before
    uint64_t infinity;
    uint64_t running_sum;
};

bool
fractional_histogram__init(struct FractionalHistogram *me, const uint64_t length);

/// @brief  Insert a non-infinite, scaled index. By scaled, I mean that the
///         index represents multiple elements.
/// @note   This is used for SHARDS.
bool
fractional_histogram__insert_scaled_finite(struct FractionalHistogram *me,
                                           const uint64_t start,
                                           const uint64_t inclusive_end,
                                           const uint64_t scale);

bool
fractional_histogram__insert_scaled_infinite(struct FractionalHistogram *me, const uint64_t scale);

void
fractional_histogram__print_sparse(struct FractionalHistogram *me);

void
fractional_histogram__destroy(struct FractionalHistogram *me);
