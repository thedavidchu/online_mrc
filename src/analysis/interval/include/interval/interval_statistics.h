#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// @note   A reuse_{distance,time} of UINT64_MAX is the same as infinite.
struct IntervalStatisticsItem {
    uint64_t reuse_distance;
    uint64_t reuse_time;
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

bool
IntervalStatistics__append(struct IntervalStatistics *const me,
                           uint64_t const reuse_distance,
                           uint64_t const reuse_time);

bool
IntervalStatistics__save(struct IntervalStatistics const *const me,
                         char const *const path);

void
IntervalStatistics__destroy(struct IntervalStatistics *const me);
