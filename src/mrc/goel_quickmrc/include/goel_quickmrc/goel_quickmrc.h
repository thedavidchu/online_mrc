#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "miss_rate_curve/miss_rate_curve.h"
#include "types/entry_type.h"

struct cache;

struct GoelQuickMRC {
    struct cache *cache;

    uint64_t num_entries_seen;
};

bool
GoelQuickMRC__init(struct GoelQuickMRC *me,
                   const int log_max_keys,
                   const int log_hist_buckets,
                   const int log_qmrc_buckets,
                   const int log_epoch_limit,
                   const double shards_sampling_ratio);

bool
GoelQuickMRC__access_item(struct GoelQuickMRC *me, EntryType entry);

void
GoelQuickMRC__post_process(struct GoelQuickMRC *me);

void
GoelQuickMRC__print_histogram_as_json(struct GoelQuickMRC *me);

bool
GoelQuickMRC__to_mrc(struct MissRateCurve *mrc, struct GoelQuickMRC *me);

void
GoelQuickMRC__destroy(struct GoelQuickMRC *me);
