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
QuickMRC__init(struct QuickMRC *const me,
               double const sampling_ratio,
               uint64_t const default_num_buckets,
               uint64_t const max_bucket_size,
               uint64_t const histogram_num_bins,
               uint64_t const histogram_bin_size,
               enum HistogramOutOfBoundsMode const out_of_bounds_mode);

bool
QuickMRC__access_item(struct QuickMRC *const me, EntryType const entry);

bool
QuickMRC__post_process(struct QuickMRC *const me);

bool
QuickMRC__to_mrc(struct QuickMRC const *const me,
                 struct MissRateCurve *const mrc);

void
QuickMRC__print_histogram_as_json(struct QuickMRC const *const me);

void
QuickMRC__destroy(struct QuickMRC *const me);

bool
QuickMRC__get_histogram(struct QuickMRC const *const me,
                        struct Histogram const **const histogram);
