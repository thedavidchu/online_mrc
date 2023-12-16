#pragma once
#include <glib.h>
#include <stdint.h>

#include "hash/types.h"
#include "histogram/basic_histogram.h"
#include "priority_queue/splay_priority_queue.h"
#include "tree/naive_tree.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

struct FixedSizeShardsReuseStack {
    struct Tree tree;
    GHashTable *hash_table;
    struct BasicHistogram histogram;
    struct SplayPriorityQueue pq;
    TimeStampType current_time_stamp;
    Hash64BitType threshold;
    uint64_t scale;
};

/// @brief  Initialize the fixed-size SHARDS data structure.
/// @param  starting_scale: This is the factor by which we divide the maximum
///                         hash size. We use this when scaling the histogram.
/// @param  max_size    :   The maximum number of elements that we will track.
///                         Additional elements will be removed.
bool
fixed_size_shards__init(struct FixedSizeShardsReuseStack *me,
                        const uint64_t starting_scale,
                        const uint64_t max_size,
                        const uint64_t max_num_unique_entries);

void
fixed_size_shards__access_item(struct FixedSizeShardsReuseStack *me, EntryType entry);

void
fixed_size_shards__print_sparse_histogram(struct FixedSizeShardsReuseStack *me);

void
fixed_size_shards__destroy(struct FixedSizeShardsReuseStack *me);
