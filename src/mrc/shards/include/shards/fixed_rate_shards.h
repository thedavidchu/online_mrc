#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "histogram/histogram.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "types/entry_type.h"

#ifdef INTERVAL_STATISTICS
#include "interval_statistics/interval_statistics.h"
#endif

struct FixedRateShards {
    struct Olken olken;
    // I cannot const qualify this because I need to set it and it is
    // stack memory, so the discussion in [1] doesn't apply.
    // It is times like these that I wish I were using Rust, or another
    // such language that doesn't require bending over backwards to
    // initialize this variable after it has been malloced. See my
    // 'init' function if you're curious what I mean!
    // [1] StackOverflow answer:
    // https://stackoverflow.com/questions/9691404/how-to-initialize-const-in-a-struct-in-c-with-malloc
    double sampling_ratio;
    uint64_t threshold;
    uint64_t scale;

    // SHARDS Adjustment Parameters
    bool adjustment;
    uint64_t num_entries_seen;
    uint64_t num_entries_processed;

#ifdef INTERVAL_STATISTICS
    struct IntervalStatistics istats;
#endif
};

/// @brief  Initialize the structures needed for fixed-size SHARDS.
/// @param  me:
/// @param  max_num_unique_entries:
/// @param  sampling_ratio: this is the ratio that SHARDS samples
/// @param  adjustment: whether to perform the SHARDS adjustment (default
///             should be true according to Waldspurger!)
bool
FixedRateShards__init(struct FixedRateShards *const me,
                      const double sampling_ratio,
                      const uint64_t histogram_num_bins,
                      const uint64_t histogram_bin_size,
                      const bool adjustment);

/// @brief  See 'FixedRateShards__init'.
/// @note   This interface is less stable than 'FixedRateShards__init'.
bool
FixedRateShards__init_full(
    struct FixedRateShards *const me,
    double const sampling_ratio,
    size_t const histogram_num_bins,
    size_t const histogram_bin_size,
    enum HistogramOutOfBoundsMode const out_of_bounds_mode,
    bool const adjustment);

void
FixedRateShards__access_item(struct FixedRateShards *me, EntryType entry);

void
FixedRateShards__post_process(struct FixedRateShards *me);

bool
FixedRateShards__to_mrc(struct FixedRateShards const *const me,
                        struct MissRateCurve *const mrc);

void
FixedRateShards__print_histogram_as_json(struct FixedRateShards *me);

void
FixedRateShards__destroy(struct FixedRateShards *me);

bool
FixedRateShards__get_histogram(struct FixedRateShards *const me,
                               struct Histogram const **const histogram)
{
    if (me == NULL) {
        return false;
    }
    return Olken__get_histogram(&me->olken, histogram);
}
