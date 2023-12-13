#pragma once

#include <stdbool.h>
#include <stddef.h>

/// @note   I assume no overflow in any of these values!
struct BasicHistogram {
    size_t *histogram;
    /// Maximum allowable length of the histogram
    size_t length;
    /// We have seen this before, but we do not track stacks this large
    size_t false_infinity;
    /// We have not seen this before
    size_t infinity;
    size_t running_sum;
};

const size_t DEFAULT_MAX_HISTOGRAM_LENGTH = 1 << 20;

bool
basic_histogram_init(struct BasicHistogram *me, const size_t length);

bool
basic_histogram_insert_finite(struct BasicHistogram *me, const size_t index);

bool
basic_histogram_insert_infinite(struct BasicHistogram *me);

void
basic_histogram_print(struct BasicHistogram *me);

void
basic_histogram_destroy(struct BasicHistogram *me);
