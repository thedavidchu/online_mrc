#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>
#include <pthread.h>

#include "histogram/histogram.h"
#include "lookup/sampled_hash_table.h"
#include "quickmrc/buckets.h"
#include "types/entry_type.h"

struct BucketedQuickMRC {
    struct SampledHashTable hash_table;
    struct QuickMRCBuckets buckets;
    struct Histogram histogram;

    // Number of entries that we have seen, regardless of whether it is above or
    // below the SHARDS threshold.
    uint64_t total_entries_seen;
    // Number of entries at or below the SHARDS theshold.
    uint64_t total_entries_processed;

    // SHARDS sampling
    uint64_t threshold;
    uint64_t scale;
    // TODO
    // - Ghost cache + capacity
};

bool
BucketedQuickMRC__init(struct BucketedQuickMRC *me,
                       const uint64_t default_num_buckets,
                       const uint64_t max_bucket_size,
                       const uint64_t histogram_length,
                       const double sampling_ratio,
                       const uint64_t max_size);

bool
BucketedQuickMRC__access_item(struct BucketedQuickMRC *me, EntryType entry);

void
BucketedQuickMRC__print_histogram_as_json(struct BucketedQuickMRC *me);

void
BucketedQuickMRC__destroy(struct BucketedQuickMRC *me);
