#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "histogram/histogram.h"
#include "lookup/evicting_hash_table.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "tree/types.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

#ifdef INTERVAL_STATISTICS
#include "interval_statistics/interval_statistics.h"
#endif
#ifdef THRESHOLD_STATISTICS
#include "statistics/statistics.h"
#endif
#ifdef PROFILE_STATISTICS
#include "profile/profile.h"
#endif

struct EvictingMap {
    struct Tree tree;
    struct EvictingHashTable hash_table;
    struct Histogram histogram;
    TimeStampType current_time_stamp;
    struct Dictionary const *dictionary;
#ifdef INTERVAL_STATISTICS
    struct IntervalStatistics istats;
#endif
#ifdef THRESHOLD_STATISTICS
    struct Statistics stats;
#endif
#ifdef PROFILE_STATISTICS
    // NOTE Reading the TSC severely impacts performance, so it is best
    //      to only measure a single part at a time.
    struct ProfileStatistics prof_stats;
#endif
};

bool
EvictingMap__init(struct EvictingMap *const me,
                  double const init_sampling_ratio,
                  uint64_t const num_hash_buckets,
                  uint64_t const histogram_num_bins,
                  uint64_t const histogram_bin_size);

bool
EvictingMap__init_full(struct EvictingMap *const me,
                       double const init_sampling_ratio,
                       uint64_t const num_hash_buckets,
                       uint64_t const histogram_num_bins,
                       uint64_t const histogram_bin_size,
                       enum HistogramOutOfBoundsMode const out_of_bounds_mode,
                       struct Dictionary const *const dictionary);

bool
EvictingMap__access_item(struct EvictingMap *me, EntryType entry);

void
EvictingMap__refresh_threshold(struct EvictingMap *me);

bool
EvictingMap__post_process(struct EvictingMap *me);

bool
EvictingMap__to_mrc(struct EvictingMap const *const me,
                    struct MissRateCurve *const mrc);

void
EvictingMap__print_histogram_as_json(struct EvictingMap *me);

void
EvictingMap__destroy(struct EvictingMap *me);

bool
EvictingMap__get_histogram(struct EvictingMap const *const me,
                           struct Histogram const **const histogram);
