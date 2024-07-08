#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "hash/MyMurmurHash3.h"
#include "histogram/histogram.h"
#ifdef INTERVAL_STATISTICS
#include "interval_statistics/interval_statistics.h"
#endif
#include "logger/logger.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "math/positive_ceiling_divide.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "shards/fixed_size_shards.h"
#include "shards/fixed_size_shards_sampler.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

bool
FixedSizeShards__init(struct FixedSizeShards *me,
                      const double starting_sampling_ratio,
                      const uint64_t max_size,
                      const uint64_t histogram_num_bins,
                      const uint64_t histogram_bin_size)
{
    if (me == NULL || starting_sampling_ratio <= 0.0 ||
        1.0 < starting_sampling_ratio || max_size == 0) {
        LOGGER_WARN("bad input");
        return false;
    }

    if (!Olken__init(&me->olken, histogram_num_bins, histogram_bin_size)) {
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
    return true;
cleanup:
    FixedSizeShards__destroy(me);
    return false;
}

static void
evict_item(void *eviction_data, EntryType entry)
{
    Olken__remove_item(eviction_data, entry);
}

bool
FixedSizeShards__access_item(struct FixedSizeShards *me, EntryType entry)
{
    // NOTE I use the nullness of the hash table as a proxy for whether this
    //      data structure has been initialized.
    if (me == NULL) {
        return false;
    }

    if (!FixedSizeShardsSampler__sample(&me->sampler, entry)) {
#ifdef INTERVAL_STATISTICS
        IntervalStatistics__append_unsampled(&me->istats);
#endif
        return false;
    }

    struct LookupReturn r = HashTable__lookup(&me->olken.hash_table, entry);
    if (r.success) {
        uint64_t distance = Olken__update_stack(&me->olken, entry, r.timestamp);
        if (distance == UINT64_MAX) {
            return false;
        }
#ifdef INTERVAL_STATISTICS
        IntervalStatistics__append_scaled(&me->istats,
                                          distance,
                                          me->sampler.scale,
                                          me->olken.current_time_stamp -
                                              r.timestamp - 1);
#endif
        Histogram__insert_scaled_finite(&me->olken.histogram,
                                        distance,
                                        me->sampler.scale);
    } else {
        if (!FixedSizeShardsSampler__insert(&me->sampler,
                                            entry,
                                            evict_item,
                                            &me->olken)) {
            return false;
        }
        if (!Olken__insert_stack(&me->olken, entry)) {
            return false;
        }
#ifdef INTERVAL_STATISTICS
        IntervalStatistics__append_infinity(&me->istats);
#endif
        Histogram__insert_scaled_infinite(&me->olken.histogram,
                                          me->sampler.scale);
    }
    return true;
}

void
FixedSizeShards__post_process(struct FixedSizeShards *me)
{
    UNUSED(me);
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
    *me = (struct FixedSizeShards){0};
}
