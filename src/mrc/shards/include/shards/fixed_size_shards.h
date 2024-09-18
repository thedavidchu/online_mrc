#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#ifdef INTERVAL_STATISTICS
#include "interval_statistics/interval_statistics.h"
#endif
#include "lookup/dictionary.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "shards/fixed_size_shards_sampler.h"
#include "types/entry_type.h"

#ifdef THRESHOLD_STATISTICS
#include "statistics/statistics.h"
#endif
#ifdef PROFILE_STATISTICS
#include "profile/profile.h"
#endif

struct FixedSizeShards {
    struct Olken olken;
    struct FixedSizeShardsSampler sampler;
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
    struct ProfileStatistics prof_stats_fast;
    struct ProfileStatistics prof_stats_slow;
#endif
};

/// @brief  Initialize the fixed-size SHARDS data structure.
/// @param  starting_scale: This is the original ratio at which we sample.
/// @param  max_size    :   The maximum number of elements that we will track.
///                         Additional elements will be removed.
bool
FixedSizeShards__init(struct FixedSizeShards *const me,
                      double const starting_sampling_ratio,
                      size_t const max_size,
                      size_t const histogram_num_bins,
                      size_t const histogram_bin_size);

/// @brief  See 'FixedSizeShards__init'.
/// @note   The interface is less stable than 'FixedSizeShards__init'.
bool
FixedSizeShards__init_full(
    struct FixedSizeShards *const me,
    double const starting_sampling_ratio,
    size_t const max_size,
    size_t const histogram_num_bins,
    size_t const histogram_bin_size,
    enum HistogramOutOfBoundsMode const out_of_bounds_mode,
    struct Dictionary const *const dictionary);

bool
FixedSizeShards__access_item(struct FixedSizeShards *me, EntryType entry);

bool
FixedSizeShards__post_process(struct FixedSizeShards *me);

bool
FixedSizeShards__to_mrc(struct FixedSizeShards const *const me,
                        struct MissRateCurve *const mrc);

void
FixedSizeShards__print_histogram_as_json(struct FixedSizeShards *me);

void
FixedSizeShards__destroy(struct FixedSizeShards *me);

bool
FixedSizeShards__get_histogram(struct FixedSizeShards *const me,
                               struct Histogram const **const histogram);
