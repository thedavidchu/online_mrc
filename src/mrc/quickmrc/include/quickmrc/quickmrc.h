#pragma once

#include <bits/stdint-uintn.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>
#include <pthread.h>

#include "histogram/histogram.h"
#include "lookup/hash_table.h"
#include "quickmrc/buckets.h"
#include "types/entry_type.h"

struct QuickMRC {
    struct HashTable hash_table;
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
QuickMRC__init(struct QuickMRC *me,
               const uint64_t default_num_buckets,
               const uint64_t max_bucket_size,
               const uint64_t histogram_length,
               const double sampling_ratio);

bool
QuickMRC__access_item(struct QuickMRC *me, EntryType entry);

void
QuickMRC__print_histogram_as_json(struct QuickMRC *me);

void
QuickMRC__destroy(struct QuickMRC *me);
