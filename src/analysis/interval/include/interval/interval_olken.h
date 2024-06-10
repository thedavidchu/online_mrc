#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "tree/types.h"

/// @note   A reuse_{distance,time} of UINT64_MAX is the same as infinite.
struct ReuseStatistics {
    uint64_t reuse_distance;
    uint64_t reuse_time;
};

struct IntervalOlken {
    struct HashTable reuse_lookup;
    struct Tree lru_stack;
    size_t current_timestamp;

    // This is a buffer to collect the reuse statistics (time and distance).
    struct ReuseStatistics *stats;
    // The length of the stats.
    size_t length;
};

bool
IntervalOlken__init(struct IntervalOlken *me, size_t const length);

bool
IntervalOlken__access_item(struct IntervalOlken *me, EntryType const entry);

void
IntervalOlken__destroy(struct IntervalOlken *me);
