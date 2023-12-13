#pragma once

#include <stdbool.h>
#include <stdint.h>

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

const uint64_t DEFAULT_MAX_HISTOGRAM_LENGTH = 1 << 20;

bool
basic_histogram_init(struct BasicHistogram *me, const uint64_t length);

bool
basic_histogram_insert_finite(struct BasicHistogram *me, const uint64_t index);

bool
basic_histogram_insert_infinite(struct BasicHistogram *me);

void
basic_histogram_print(struct BasicHistogram *me);

void
basic_histogram_destroy(struct BasicHistogram *me);
