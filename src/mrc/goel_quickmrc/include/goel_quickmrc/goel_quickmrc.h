#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "histogram/histogram.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "types/entry_type.h"

struct cache;

struct GoelQuickMRC {
    struct cache *cache;
    // This is empty until we fill it. This is simply for a cleaner
    // interface so we don't have to manage as much dynamic memory.
    struct Histogram histogram;

    // SHARDS
    double sampling_ratio;
    uint64_t threshold;
    uint64_t scale;
    bool shards_adjustment;

    // Post-Processing for SHARDS and MRC Generation
    uint64_t num_entries_seen;
    uint64_t num_entries_processed;
};

bool
GoelQuickMRC__init(struct GoelQuickMRC *me,
                   const double shards_sampling_ratio,
                   const int max_keys,
                   const int log_hist_buckets,
                   const int log_qmrc_buckets,
                   const int log_epoch_limit,
                   const bool shards_adjustment);

bool
GoelQuickMRC__access_item(struct GoelQuickMRC *me, EntryType entry);

bool
GoelQuickMRC__post_process(struct GoelQuickMRC *me);

void
GoelQuickMRC__print_histogram_as_json(struct GoelQuickMRC *me);

bool
GoelQuickMRC__save_sparse_histogram(struct GoelQuickMRC const *const me,
                                    char const *const path);

bool
GoelQuickMRC__to_mrc(struct GoelQuickMRC const *const me,
                     struct MissRateCurve *const mrc);

void
GoelQuickMRC__destroy(struct GoelQuickMRC *me);

bool
GoelQuickMRC__get_histogram(struct GoelQuickMRC *const me,
                            struct Histogram const **const histogram);
