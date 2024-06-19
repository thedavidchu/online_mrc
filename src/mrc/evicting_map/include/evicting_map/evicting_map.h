#pragma once

#include <stdint.h>

#include "histogram/histogram.h"
#include "lookup/evicting_hash_table.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "tree/types.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "unused/mark_unused.h"

struct EvictingMap {
    struct Tree tree;
    struct EvictingHashTable hash_table;
    struct Histogram histogram;
    TimeStampType current_time_stamp;
};

bool
BucketedShards__init(struct EvictingMap *me,
                     const double init_sampling_ratio,
                     const uint64_t num_hash_buckets,
                     const uint64_t histogram_num_bins,
                     const uint64_t histogram_bin_size);

void
BucketedShards__access_item(struct EvictingMap *me, EntryType entry);

void
BucketedShards__refresh_threshold(struct EvictingMap *me);

bool
BucketedShards__post_process(struct EvictingMap *me);

bool
EvictingMap__to_mrc(struct EvictingMap const *const me,
                    struct MissRateCurve *const mrc);

void
BucketedShards__print_histogram_as_json(struct EvictingMap *me);

void
BucketedShards__destroy(struct EvictingMap *me);
