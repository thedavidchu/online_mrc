#pragma once

#include <stdbool.h>
#include <stdint.h>

/// @brief  This histogram tracks (potentially scaled) equal-sized values.
/// @note   I assume no overflow in any of these values!
struct FractionalHistogram {
    double *histogram;
    /// Each bin in the histogram represents this many values
    uint64_t bin_size;
    /// Number of bins in the histogram
    uint64_t num_bins;
    /// We have seen this before, but we do not track stacks this large
    double false_infinity;
    /// We have not seen this before
    uint64_t infinity;
    uint64_t running_sum;
};

bool
FractionalHistogram__init(struct FractionalHistogram *me,
                          const uint64_t num_bins,
                          const uint64_t bin_size);

/// @brief  Insert a non-infinite, scaled index. By scaled, I mean that the
///         index represents multiple elements.
/// @note   This is used for SHARDS.
bool
FractionalHistogram__insert_scaled_finite(struct FractionalHistogram *me,
                                          const uint64_t start,
                                          const uint64_t range,
                                          const uint64_t scale);

bool
FractionalHistogram__insert_scaled_infinite(struct FractionalHistogram *me,
                                            const uint64_t scale);

void
FractionalHistogram__print_as_json(struct FractionalHistogram *me);

bool
FractionalHistogram__exactly_equal(struct FractionalHistogram *me,
                                   struct FractionalHistogram *other);

bool
FractionalHistogram__validate(struct FractionalHistogram *me);

void
FractionalHistogram__destroy(struct FractionalHistogram *me);
