#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// @note   A reuse_{distance,time} of UINT64_MAX is the same as infinite.
struct ReuseStatisticsItem {
    uint64_t reuse_distance;
    uint64_t reuse_time;
};

struct ReuseStatistics {
    // This is a buffer to collect the reuse statistics (time and distance).
    struct ReuseStatisticsItem *stats;
    // The length of the stats.
    size_t length;
    size_t capacity;
};

bool
ReuseStatistics__init(struct ReuseStatistics *const me,
                      size_t const init_capacity);

bool
ReuseStatistics__append(struct ReuseStatistics *const me,
                        uint64_t const reuse_distance,
                        uint64_t const reuse_time);

bool
ReuseStatistics__save(struct ReuseStatistics const *const me,
                      char const *const path);

void
ReuseStatistics__destroy(struct ReuseStatistics *const me);
