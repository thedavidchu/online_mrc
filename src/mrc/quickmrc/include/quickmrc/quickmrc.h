#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>
#include <pthread.h>

#include "histogram/basic_histogram.h"
#include "lookup/parallel_hash_table.h"
#include "quickmrc/buckets.h"
#include "types/entry_type.h"

struct QuickMrc {
    struct ParallelHashTable hash_table;
    struct QuickMRCBuckets buckets;
    struct BasicHistogram histogram;

    // Number of entries that we have seen, regardless of whether it is above or
    // below the SHARDS threshold.
    uint64_t total_entries_seen;
    // Number of entries at or below the SHARDS theshold.
    uint64_t total_entries_processed;

    // TODO
    // - Ghost cache + capacity
};

bool
quickmrc__init(struct QuickMrc *me,
               uint64_t default_num_buckets,
               uint64_t max_bucket_size,
               uint64_t histogram_length);

bool
quickmrc__access_item(struct QuickMrc *me, EntryType entry);

void
quickmrc__print_histogram_as_json(struct QuickMrc *me);

void
quickmrc__destroy(struct QuickMrc *me);
