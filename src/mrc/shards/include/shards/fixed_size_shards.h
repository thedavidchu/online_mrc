#pragma once
#include <glib.h>
#include <stdint.h>

#include "hash/types.h"
#include "histogram/basic_histogram.h"
#include "priority_queue/splay_priority_queue.h"
#include "tree/types.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

struct FixedSizeShards {
    struct Tree tree;
    GHashTable *hash_table;
    struct BasicHistogram histogram;
    struct SplayPriorityQueue pq;
    TimeStampType current_time_stamp;
    Hash64BitType threshold;
    uint64_t scale;
};

/// @brief  Initialize the fixed-size SHARDS data structure.
/// @param  starting_scale: This is the original ratio at which we sample.
/// @param  max_size    :   The maximum number of elements that we will track.
///                         Additional elements will be removed.
bool
FixedSizeShards__init(struct FixedSizeShards *me,
                        const double starting_sampling_ratio,
                        const uint64_t max_size,
                        const uint64_t max_num_unique_entries);

void
FixedSizeShards__access_item(struct FixedSizeShards *me,
                               EntryType entry);

void
FixedSizeShards__print_histogram_as_json(
    struct FixedSizeShards *me);

void
FixedSizeShards__destroy(struct FixedSizeShards *me);
