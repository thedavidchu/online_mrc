/** @brief  My implementation of QuickMRC. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>
#include <pthread.h>

#include "histogram/histogram.h"
#include "lookup/hash_table.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "quickmrc/quickmrc_buckets.h"
#include "shards/fixed_rate_shards_sampler.h"
#include "types/entry_type.h"

struct QuickMRC {
    struct FixedRateShardsSampler sampler;
    struct HashTable hash_table;
    struct qmrc buckets;
    struct Histogram histogram;
};

bool
QuickMRC__init(struct QuickMRC *me,
               const double sampling_ratio,
               const uint64_t default_num_buckets,
               const uint64_t max_bucket_size,
               const uint64_t histogram_num_bins,
               const uint64_t histogram_bin_size);

bool
QuickMRC__access_item(struct QuickMRC *me, EntryType entry);

void
QuickMRC__post_process(struct QuickMRC *me);

bool
QuickMRC__to_mrc(struct QuickMRC const *const me,
                 struct MissRateCurve *const mrc);

void
QuickMRC__print_histogram_as_json(struct QuickMRC const *const me);

void
QuickMRC__destroy(struct QuickMRC *const me);
