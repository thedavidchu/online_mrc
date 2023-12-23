#pragma once

#include <stdbool.h>
#include <stdint.h>

/// @brief  This histogram tracks (potentially scaled) equal-sized values.
/// @note   I assume no overflow in any of these values!
struct BasicHistogram {
    uint64_t *histogram;
    /// Maximum allowable length of the histogram
    uint64_t length;
    /// We have seen this before, but we do not track stacks this large
    uint64_t false_infinity;
    /// We have not seen this before
    uint64_t infinity;
    uint64_t running_sum;
};

bool
basic_histogram__init(struct BasicHistogram *me, const uint64_t length);

bool
basic_histogram__insert_finite(struct BasicHistogram *me, const uint64_t index);

/// @brief  Insert a non-infinite, scaled index. By scaled, I mean that the
///         index represents multiple elements.
/// @note   This is used for SHARDS.
bool
basic_histogram__insert_scaled_finite(struct BasicHistogram *me,
                                      const uint64_t index,
                                      const uint64_t scale);

bool
basic_histogram__insert_infinite(struct BasicHistogram *me);

bool
basic_histogram__insert_scaled_infinite(struct BasicHistogram *me,
                                        const uint64_t scale);

bool
basic_histogram__exactly_equal(struct BasicHistogram *me,
                               struct BasicHistogram *other);

void
basic_histogram__print_as_json(struct BasicHistogram *me);

void
basic_histogram__destroy(struct BasicHistogram *me);
