#include <assert.h>
#include <math.h>
#include <stdbool.h> // bool
#include <stdint.h>  // uint64_t
#include <stdio.h>
#include <stdlib.h>

#include "histogram/histogram.h"
#ifdef INTERVAL_STATISTICS
#include "interval_statistics/interval_statistics.h"
#endif
#include "logger/logger.h"
#include "lookup/evicting_hash_table.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "types/value_type.h"
#include "unused/mark_unused.h"

#include "evicting_map/evicting_map.h"

bool
EvictingMap__init(struct EvictingMap *me,
                  const double init_sampling_ratio,
                  const uint64_t num_hash_buckets,
                  const uint64_t histogram_num_bins,
                  const uint64_t histogram_bin_size)
{
    if (me == NULL)
        return false;
    if (!tree__init(&me->tree))
        goto cleanup;
    if (!EvictingHashTable__init(&me->hash_table,
                                 num_hash_buckets,
                                 init_sampling_ratio))
        goto cleanup;
    if (!Histogram__init(&me->histogram,
                         histogram_num_bins,
                         histogram_bin_size,
                         false))
        goto cleanup;
#ifdef INTERVAL_STATISTICS
    if (!IntervalStatistics__init(&me->istats, histogram_num_bins)) {
        goto cleanup;
    }
#endif
    me->current_time_stamp = 0;
    return true;

cleanup:
    EvictingMap__destroy(me);
    return false;
}

/// @brief  Do no work (besides simple book-keeping).
static inline void
handle_ignored(struct EvictingMap *me,
               struct SampledTryPutReturn s,
               TimeStampType value)
{
    UNUSED(s);
    UNUSED(value);
    assert(me != NULL);

#ifdef INTERVAL_STATISTICS
    IntervalStatistics__append_unsampled(&me->istats);
#endif
    // NOTE We increment the current_time_stamp to be consistent
    //      with Olken during our interval analysis.
    ++me->current_time_stamp;
}

/// @brief  Insert a new element into the hash table without eviction.
static inline void
handle_inserted(struct EvictingMap *me,
                struct SampledTryPutReturn s,
                TimeStampType value)
{
    UNUSED(s);
    assert(me != NULL);

    const uint64_t scale =
        EvictingHashTable__estimate_scale_factor(&me->hash_table);
    bool r = false;
    MAYBE_UNUSED(r);

    r = tree__sleator_insert(&me->tree, value);
    assert(r);
    Histogram__insert_scaled_infinite(&me->histogram, scale == 0 ? 1 : scale);
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__append_infinity(&me->istats);
#endif
    ++me->current_time_stamp;
}

/// @brief  Insert a new element into the hash table but replace an old
///         element.
static inline void
handle_replaced(struct EvictingMap *me,
                struct SampledTryPutReturn s,
                TimeStampType timestamp)
{
    assert(me != NULL);

    const uint64_t scale =
        EvictingHashTable__estimate_scale_factor(&me->hash_table);
    bool r = false;
    MAYBE_UNUSED(r);

    r = tree__sleator_remove(&me->tree, s.old_value);
    assert(r);
    r = tree__sleator_insert(&me->tree, timestamp);
    assert(r);

    Histogram__insert_scaled_infinite(&me->histogram, scale == 0 ? 1 : scale);
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__append_infinity(&me->istats);
#endif
    ++me->current_time_stamp;
}

/// @brief  Update an existing element in the hash table.
static inline void
handle_updated(struct EvictingMap *me,
               struct SampledTryPutReturn s,
               TimeStampType timestamp)
{
    assert(me != NULL);

    const uint64_t scale =
        EvictingHashTable__estimate_scale_factor(&me->hash_table);
    bool r = false;
    uint64_t distance = 0;
    MAYBE_UNUSED(r);

    distance = tree__reverse_rank(&me->tree, (KeyType)s.old_value);
    r = tree__sleator_remove(&me->tree, s.old_value);
    assert(r);
    r = tree__sleator_insert(&me->tree, timestamp);
    assert(r);

    Histogram__insert_scaled_finite(&me->histogram,
                                    distance,
                                    scale == 0 ? 1 : scale);
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__append(&me->istats,
                               (double)distance * (scale == 0 ? 1 : scale),
                               (double)me->current_time_stamp - timestamp - 1);
#endif
    ++me->current_time_stamp;
}

void
EvictingMap__access_item(struct EvictingMap *me, EntryType entry)
{
    if (me == NULL)
        return;
    ValueType timestamp = me->current_time_stamp;
    struct SampledTryPutReturn r =
        EvictingHashTable__try_put(&me->hash_table, entry, timestamp);
    switch (r.status) {
    case SAMPLED_IGNORED:
        /* Do no work -- this is like SHARDS */
        handle_ignored(me, r, timestamp);
        break;
    case SAMPLED_INSERTED:
        handle_inserted(me, r, timestamp);
        break;
    case SAMPLED_REPLACED:
        handle_replaced(me, r, timestamp);
        break;
    case SAMPLED_UPDATED:
        handle_updated(me, r, timestamp);
        break;
    default:
        assert(0 && "impossible");
    }
}

void
EvictingMap__refresh_threshold(struct EvictingMap *me)
{
    if (me == NULL)
        return;
    EvictingHashTable__refresh_threshold(&me->hash_table);
}

bool
EvictingMap__post_process(struct EvictingMap *me)
{
    UNUSED(me);
    return true;
}

bool
EvictingMap__to_mrc(struct EvictingMap const *const me,
                    struct MissRateCurve *const mrc)
{
    return MissRateCurve__init_from_histogram(mrc, &me->histogram);
}
void
EvictingMap__print_histogram_as_json(struct EvictingMap *me)
{
    if (me == NULL) {
        // Just pass on the NULL value and let the histogram deal with it. Maybe
        // this isn't very smart and will confuse future-me? Oh well!
        Histogram__print_as_json(NULL);
        return;
    }
    Histogram__print_as_json(&me->histogram);
}

void
EvictingMap__destroy(struct EvictingMap *me)
{
    if (me == NULL) {
        return;
    }
    tree__destroy(&me->tree);
    EvictingHashTable__destroy(&me->hash_table);
    Histogram__destroy(&me->histogram);
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__destroy(&me->istats);
#endif
    *me = (struct EvictingMap){0};
}
