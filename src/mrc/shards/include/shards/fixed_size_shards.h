#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "histogram/histogram.h"
#ifdef INTERVAL_STATISTICS
#include "interval_statistics/interval_statistics.h"
#endif
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "shards/fixed_size_shards_sampler.h"
#include "types/entry_type.h"

struct FixedSizeShards {
    struct Olken olken;
    struct FixedSizeShardsSampler sampler;
    struct IntervalStatistics istats;
};

/// @brief  Initialize the fixed-size SHARDS data structure.
/// @param  starting_scale: This is the original ratio at which we sample.
/// @param  max_size    :   The maximum number of elements that we will track.
///                         Additional elements will be removed.
bool
FixedSizeShards__init(struct FixedSizeShards *me,
                      const double starting_sampling_ratio,
                      const uint64_t max_size,
                      const uint64_t histogram_num_bins,
                      const uint64_t histogram_bin_size);

bool
FixedSizeShards__access_item(struct FixedSizeShards *me, EntryType entry);

void
FixedSizeShards__post_process(struct FixedSizeShards *me);

bool
FixedSizeShards__to_mrc(struct FixedSizeShards const *const me,
                        struct MissRateCurve *const mrc);

void
FixedSizeShards__print_histogram_as_json(struct FixedSizeShards *me);

void
FixedSizeShards__destroy(struct FixedSizeShards *me);
