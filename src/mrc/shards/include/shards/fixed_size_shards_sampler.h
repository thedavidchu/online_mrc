#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "priority_queue/heap.h"
#include "types/entry_type.h"

struct FixedSizeShardsSampler {
    double sampling_ratio;
    uint64_t threshold;
    uint64_t scale;

    // Fixed-Size SHARDS
    struct Heap pq;

    // SHARDS Adjustment Parameters -- not supported yet!
    bool adjustment;
    uint64_t num_entries_seen;
    // By processed, we mean "sampled"!
    uint64_t num_entries_processed;
};

bool
FixedSizeShardsSampler__init(struct FixedSizeShardsSampler *const me,
                             const double starting_sampling_ratio,
                             const uint64_t max_size,
                             bool const adjustment);

void
FixedSizeShardsSampler__destroy(struct FixedSizeShardsSampler *const me);

/// @brief  Whether to sample or not.
bool
FixedSizeShardsSampler__sample(struct FixedSizeShardsSampler *me,
                               EntryType entry);

/// @brief  Insert an item into the Fixed-Size SHARDS sampler (after we
///         have determined that we indeed want to track it!).
/// @note   I provide a hook so that in future, we can link the legacy
///         fixed size SHARDS module to this.
/// @param  eviction_hook: (void *, EntryType) -> void
///         The hook to run upon an eviction from the priority queue.
/// @param  eviction_data: void *
///         The user data required for the eviction hook.
bool
FixedSizeShardsSampler__insert(struct FixedSizeShardsSampler *me,
                               EntryType entry,
                               void (*eviction_hook)(void *data, EntryType key),
                               void *eviction_data);

/// @brief  Estimate the number of unique items we have seen.
/// @details    I think this is valid. Let's break it down into the two
///             cases as follows:
///             1. Non-full: use the fixed-rate SHARDS estimate, where
///             we multiply the scale by the number of elements we have.
///             2. Full: it's actually identical. We are capped at a
///             certain number of elements, but our scale increases!
uint64_t
FixedSizeShardsSampler__estimate_cardinality(struct FixedSizeShardsSampler *me);
