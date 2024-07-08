#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "histogram/histogram.h"

/// @note   I am safe to use a 'double' here because it can represent
///         1<<53 without loss of precision (i.e. more than 1<<48, which
///         is the size of the virtual address space). For this reason,
///         we don't actually lose any precision!
/// @note   I use 'INFINITY' to represent an element that has never been
///         seen before an 'NAN' to represent an element that is not
///         sampled.
struct IntervalStatisticsItem {
    double reuse_distance;
    double reuse_time;
};

struct IntervalStatistics {
    // This is a buffer to collect the reuse statistics (time and distance).
    struct IntervalStatisticsItem *stats;
    // The length of the stats.
    size_t length;
    size_t capacity;
};

bool
IntervalStatistics__init(struct IntervalStatistics *const me,
                         size_t const init_capacity);

void
IntervalStatistics__destroy(struct IntervalStatistics *const me);

/// @brief  Append a reuse distance/time to the interval statistics.
bool
IntervalStatistics__append_scaled(struct IntervalStatistics *const me,
                                  double const reuse_distance,
                                  double const reuse_distance_horizontal_scale,
                                  double const reuse_time);

bool
IntervalStatistics__append(struct IntervalStatistics *const me,
                           double const reuse_distance,
                           double const reuse_time);

/// @note   This function simply exists to we wrap the <math.h> file
///         so users don't need to build with the math dependency.
bool
IntervalStatistics__append_unsampled(struct IntervalStatistics *const me);

/// @note   This function simply exists to we wrap the <math.h> file
///         so users don't need to build with the math dependency.
bool
IntervalStatistics__append_infinity(struct IntervalStatistics *const me);

bool
IntervalStatistics__save(struct IntervalStatistics const *const me,
                         char const *const path);

bool
IntervalStatistics__to_histogram(struct IntervalStatistics const *const me,
                                 struct Histogram *const hist,
                                 size_t const num_bins,
                                 size_t const bin_size);
