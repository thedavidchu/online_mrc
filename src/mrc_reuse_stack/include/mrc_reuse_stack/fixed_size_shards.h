#pragma once

#include <glib.h>

struct FixedSizeShardsReuseStack {
    struct Tree tree;
    GHashTable *hash_table;
    BasicHistogram histogram;
    SplayPriorityQueue pq;
    TimeStampType current_time_stamp;
    Hash64BitType threshold;
    uint64_t scale;
};

/// @brief  Initialize the fixed-size SHARDS data structure.
/// @param  starting_scale: This is the factor by which we divide the maximum
///                         hash size. We use this when scaling the histogram.
bool
fixed_size_shards_init(struct FixedSizeShardsReuseStack *me,
                       const uint64_t starting_scale,
                       const uint64_t max_size,
                       const uint64_t histogram_length);

void
fixed_size_shards_access_item(struct FixedSizeShardsReuseStack *me, EntryType entry);

void
fixed_size_shards_print_sparse_histogram(struct OlkenReuseStack *me);

void
fixed_size_shards_destroy(struct OlkenReuseStack *me);
