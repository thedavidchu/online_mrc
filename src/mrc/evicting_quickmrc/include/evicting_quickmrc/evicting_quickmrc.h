#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "histogram/histogram.h"
#include "lookup/evicting_hash_table.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "tree/types.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

#include "qmrc.h"

#ifdef INTERVAL_STATISTICS
#include "interval_statistics/interval_statistics.h"
#endif

struct EvictingQuickMRC {
    struct EvictingHashTable hash_table;
    // This is Ashvin's QuickMRC array.
    struct qmrc qmrc;
    struct Histogram histogram;
    TimeStampType current_time_stamp;
#ifdef INTERVAL_STATISTICS
    struct IntervalStatistics istats;
#endif
};

bool
EvictingQuickMRC__init(struct EvictingQuickMRC *const me,
                       double const init_sampling_ratio,
                       uint64_t const num_hash_buckets,
                       uint64_t const histogram_num_bins,
                       uint64_t const histogram_bin_size,
                       enum HistogramOutOfBoundsMode const out_of_bounds_mode);

bool
EvictingQuickMRC__access_item(struct EvictingQuickMRC *me, EntryType entry);

void
EvictingQuickMRC__refresh_threshold(struct EvictingQuickMRC *me);

bool
EvictingQuickMRC__post_process(struct EvictingQuickMRC *me);

bool
EvictingQuickMRC__to_mrc(struct EvictingQuickMRC const *const me,
                         struct MissRateCurve *const mrc);

void
EvictingQuickMRC__print_histogram_as_json(struct EvictingQuickMRC *me);

void
EvictingQuickMRC__destroy(struct EvictingQuickMRC *me);

bool
EvictingQuickMRC__get_histogram(struct EvictingQuickMRC const *const me,
                                struct Histogram const **const histogram);
