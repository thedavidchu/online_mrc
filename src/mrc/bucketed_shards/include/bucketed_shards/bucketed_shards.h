#pragma once

#include <stdint.h>

#include "hash/types.h"
#include "histogram/histogram.h"
#include "tree/types.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "lookup/sampled_hash_table.h"

struct BucketedShards {
    struct Tree tree;
    struct SampledHashTable hash_table;
    struct Histogram histogram;
    TimeStampType current_time_stamp;
};

bool
BucketedShards__init(struct BucketedShards *me,
                     const uint64_t num_hash_buckets,
                     const uint64_t max_num_unique_entries,
                     const double init_sampling_ratio);

void
BucketedShards__access_item(struct BucketedShards *me, EntryType entry);

void
BucketedShards__print_histogram_as_json(struct BucketedShards *me);

void
BucketedShards__destroy(struct BucketedShards *me);
