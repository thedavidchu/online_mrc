#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "histogram/histogram.h"
#ifdef INTERVAL_STATISTICS
#include "interval_statistics/interval_statistics.h"
#endif
#include "logger/logger.h"
#include "lookup/lookup.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "shards/fixed_size_shards.h"
#include "shards/fixed_size_shards_sampler.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "unused/mark_unused.h"

#include "shards/fixed_size_shards.h"

/// NOTE    This has to be below the fixed-size SHARDS include so that
///         it sees the macro definition.
#ifdef THRESHOLD_STATISTICS
#include "statistics/statistics.h"
#endif

#define THRESHOLD_SAMPLING_PERIOD (1 << 20)

static bool
initialize(struct FixedSizeShards *const me,
           double const starting_sampling_ratio,
           size_t const max_size,
           size_t const histogram_num_bins,
           size_t const histogram_bin_size,
           enum HistogramOutOfBoundsMode const out_of_bounds_mode)
{
    if (me == NULL || starting_sampling_ratio <= 0.0 ||
        1.0 < starting_sampling_ratio || max_size == 0) {
        LOGGER_WARN("bad input");
        return false;
    }

    if (!Olken__init_full(&me->olken,
                          histogram_num_bins,
                          histogram_bin_size,
                          out_of_bounds_mode)) {
        LOGGER_WARN("failed to initialize Olken");
        goto cleanup;
    }
    if (!FixedSizeShardsSampler__init(&me->sampler,
                                      starting_sampling_ratio,
                                      max_size,
                                      false)) {
        LOGGER_WARN("failed to initialize fixed-size SHARDS sampler");
        goto cleanup;
    }
#ifdef INTERVAL_STATISTICS
    if (!IntervalStatistics__init(&me->istats, histogram_num_bins)) {
        goto cleanup;
    }
#endif
#ifdef THRESHOLD_STATISTICS
    if (!Statistics__init(&me->stats, 2)) {
        goto cleanup;
    }
#endif
    return true;
cleanup:
    FixedSizeShards__destroy(me);
    return false;
}

bool
FixedSizeShards__init(struct FixedSizeShards *const me,
                      double const starting_sampling_ratio,
                      size_t const max_size,
                      size_t const histogram_num_bins,
                      size_t const histogram_bin_size)
{
    return initialize(me,
                      starting_sampling_ratio,
                      max_size,
                      histogram_num_bins,
                      histogram_bin_size,
                      HistogramOutOfBoundsMode__allow_overflow);
}

bool
FixedSizeShards__init_full(
    struct FixedSizeShards *const me,
    double const starting_sampling_ratio,
    size_t const max_size,
    size_t const histogram_num_bins,
    size_t const histogram_bin_size,
    enum HistogramOutOfBoundsMode const out_of_bounds_mode)
{
    return initialize(me,
                      starting_sampling_ratio,
                      max_size,
                      histogram_num_bins,
                      histogram_bin_size,
                      out_of_bounds_mode);
}

static void
unsampled_item(struct FixedSizeShards *me)
{
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__append_unsampled(&me->istats);
#endif
    Olken__ignore_entry(&me->olken);
}

static bool
update_item(struct FixedSizeShards *me,
            EntryType entry,
            TimeStampType timestamp)
{
    uint64_t distance = Olken__update_stack(&me->olken, entry, timestamp);
    if (distance == UINT64_MAX) {
        return false;
    }
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__append_scaled(&me->istats,
                                      distance,
                                      me->sampler.scale,
                                      me->olken.current_time_stamp - timestamp -
                                          1);
#endif
    Histogram__insert_scaled_finite(&me->olken.histogram,
                                    distance,
                                    me->sampler.scale);
    return true;
}

static void
evict_item(void *eviction_data, EntryType entry)
{
    bool r = Olken__remove_item(eviction_data, entry);
    assert(r);
}

static bool
insert_item(struct FixedSizeShards *me, EntryType entry)
{
    if (!FixedSizeShardsSampler__insert(&me->sampler,
                                        entry,
                                        evict_item,
                                        &me->olken)) {
        LOGGER_ERROR("fixed-size SHARDS sampler insertion failed");
        return false;
    }
    if (!Olken__insert_stack(&me->olken, entry)) {
        LOGGER_ERROR("Olken insertion failed");
        return false;
    }
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__append_infinity(&me->istats);
#endif
    Histogram__insert_scaled_infinite(&me->olken.histogram, me->sampler.scale);
    return true;
}

bool
FixedSizeShards__access_item(struct FixedSizeShards *me, EntryType entry)
{
    // NOTE I use the nullness of the hash table as a proxy for whether this
    //      data structure has been initialized.
    if (me == NULL) {
        return false;
    }
#ifdef THRESHOLD_STATISTICS
    if (me->olken.current_time_stamp % THRESHOLD_SAMPLING_PERIOD == 0) {
        uint64_t const data[] = {me->olken.current_time_stamp,
                                 me->sampler.threshold};
        Statistics__append_uint64(&me->stats, data);
    }
#endif
    if (!FixedSizeShardsSampler__sample(&me->sampler, entry)) {
        unsampled_item(me);
        return false;
    }

    struct LookupReturn r = Olken__lookup(&me->olken, entry);
    if (r.success) {
        return update_item(me, entry, r.timestamp);
    } else {
        return insert_item(me, entry);
    }
    return true;
}

bool
FixedSizeShards__post_process(struct FixedSizeShards *me)
{
    UNUSED(me);
    return true;
}

bool
FixedSizeShards__to_mrc(struct FixedSizeShards const *const me,
                        struct MissRateCurve *const mrc)
{
    return Olken__to_mrc(&me->olken, mrc);
}

void
FixedSizeShards__print_histogram_as_json(struct FixedSizeShards *me)
{
    if (me == NULL) {
        // Just pass on the NULL value and let the histogram deal with it. Maybe
        // this isn't very smart and will confuse future-me? Oh well!
        Histogram__print_as_json(NULL);
        return;
    }
    Histogram__print_as_json(&me->olken.histogram);
}

void
FixedSizeShards__destroy(struct FixedSizeShards *me)
{
    Olken__destroy(&me->olken);
    FixedSizeShardsSampler__destroy(&me->sampler);
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__destroy(&me->istats);
#endif
#ifdef THRESHOLD_STATISTICS
    Statistics__save(&me->stats, "Fixed-Size-SHARDS-stats.bin");
    Statistics__destroy(&me->stats);
#endif
    *me = (struct FixedSizeShards){0};
}

bool
FixedSizeShards__get_histogram(struct FixedSizeShards *const me,
                               struct Histogram const **const histogram)
{
    if (me == NULL) {
        return false;
    }
    return Olken__get_histogram(&me->olken, histogram);
}
