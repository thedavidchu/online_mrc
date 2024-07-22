#include <assert.h>
#include <stdbool.h> // bool
#include <stdint.h>  // uint64_t
#include <stdio.h>
#include <stdlib.h>

#include "histogram/histogram.h"
#include "qmrc.h"
#ifdef INTERVAL_STATISTICS
#include "interval_statistics/interval_statistics.h"
#endif
#include "lookup/evicting_hash_table.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "types/value_type.h"
#include "unused/mark_unused.h"

#include "evicting_quickmrc/evicting_quickmrc.h"

static bool
initialize(struct EvictingQuickMRC *const me,
           double const init_sampling_ratio,
           uint64_t const num_hash_buckets,
           uint64_t const num_qmrc__buckets,
           uint64_t const histogram_num_bins,
           uint64_t const histogram_bin_size,
           enum HistogramOutOfBoundsMode const out_of_bounds_mode)
{
    if (me == NULL)
        return false;
    if (!qmrc__init(&me->qmrc, num_hash_buckets, num_qmrc__buckets, 0))
        goto cleanup;
    if (!EvictingHashTable__init(&me->hash_table,
                                 num_hash_buckets,
                                 init_sampling_ratio))
        goto cleanup;
    if (!Histogram__init(&me->histogram,
                         histogram_num_bins,
                         histogram_bin_size,
                         out_of_bounds_mode))
        goto cleanup;
#ifdef INTERVAL_STATISTICS
    if (!IntervalStatistics__init(&me->istats, histogram_num_bins)) {
        goto cleanup;
    }
#endif
    me->current_time_stamp = 0;
    return true;

cleanup:
    EvictingQuickMRC__destroy(me);
    return false;
}

bool
EvictingQuickMRC__init(struct EvictingQuickMRC *const me,
                       const double init_sampling_ratio,
                       const uint64_t num_hash_buckets,
                       uint64_t const num_qmrc__buckets,
                       const uint64_t histogram_num_bins,
                       uint64_t const histogram_bin_size,
                       enum HistogramOutOfBoundsMode const out_of_bounds_mode)
{
    return initialize(me,
                      init_sampling_ratio,
                      num_hash_buckets,
                      num_qmrc__buckets,
                      histogram_num_bins,
                      histogram_bin_size,
                      out_of_bounds_mode);
}

/// @brief  Do no work (besides simple book-keeping).
static inline void
handle_ignored(struct EvictingQuickMRC *me,
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
handle_inserted(struct EvictingQuickMRC *me, struct SampledTryPutReturn s)
{
    UNUSED(s);
    assert(me != NULL);

    const uint64_t scale =
        EvictingHashTable__estimate_scale_factor(&me->hash_table);
    qmrc__insert(&me->qmrc);
    Histogram__insert_scaled_infinite(&me->histogram, scale == 0 ? 1 : scale);
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__append_infinity(&me->istats);
#endif
    ++me->current_time_stamp;
}

/// @brief  Insert a new element into the hash table but replace an old
///         element.
static inline void
handle_replaced(struct EvictingQuickMRC *me, struct SampledTryPutReturn s)
{
    assert(me != NULL);

    const uint64_t scale =
        EvictingHashTable__estimate_scale_factor(&me->hash_table);
    bool r = false;
    MAYBE_UNUSED(r);

    qmrc__delete(&me->qmrc, s.old_value);
    r = qmrc__insert(&me->qmrc);
    assert(r);

    Histogram__insert_scaled_infinite(&me->histogram, scale == 0 ? 1 : scale);
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__append_infinity(&me->istats);
#endif
    ++me->current_time_stamp;
}

/// @brief  Update an existing element in the hash table.
static inline void
handle_updated(struct EvictingQuickMRC *me, struct SampledTryPutReturn s)
{
    assert(me != NULL);

    const uint64_t scale =
        EvictingHashTable__estimate_scale_factor(&me->hash_table);
    bool r = false;
    uint64_t distance = 0;
    MAYBE_UNUSED(r);

    distance = qmrc__lookup(&me->qmrc, (KeyType)s.old_value);
    qmrc__delete(&me->qmrc, s.old_value);
    r = qmrc__insert(&me->qmrc);
    assert(r);

    Histogram__insert_scaled_finite(&me->histogram,
                                    distance,
                                    scale == 0 ? 1 : scale);
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__append_scaled(&me->istats,
                                      (double)distance,
                                      (double)(scale == 0 ? 1 : scale),
                                      (double)me->current_time_stamp -
                                          timestamp - 1);
#endif
    ++me->current_time_stamp;
}

bool
EvictingQuickMRC__access_item(struct EvictingQuickMRC *me, EntryType entry)
{
    if (me == NULL)
        return false;
    ValueType timestamp = me->current_time_stamp;
    struct SampledTryPutReturn r =
        EvictingHashTable__try_put(&me->hash_table, entry, timestamp);
    switch (r.status) {
    case SAMPLED_IGNORED:
        /* Do no work -- this is like SHARDS */
        handle_ignored(me, r, timestamp);
        break;
    case SAMPLED_INSERTED:
        handle_inserted(me, r);
        break;
    case SAMPLED_REPLACED:
        handle_replaced(me, r);
        break;
    case SAMPLED_UPDATED:
        handle_updated(me, r);
        break;
    default:
        assert(0 && "impossible");
    }
    return true;
}

void
EvictingQuickMRC__refresh_threshold(struct EvictingQuickMRC *me)
{
    if (me == NULL)
        return;
    EvictingHashTable__refresh_threshold(&me->hash_table);
}

bool
EvictingQuickMRC__post_process(struct EvictingQuickMRC *me)
{
    UNUSED(me);
    return true;
}

bool
EvictingQuickMRC__to_mrc(struct EvictingQuickMRC const *const me,
                         struct MissRateCurve *const mrc)
{
    return MissRateCurve__init_from_histogram(mrc, &me->histogram);
}
void
EvictingQuickMRC__print_histogram_as_json(struct EvictingQuickMRC *me)
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
EvictingQuickMRC__destroy(struct EvictingQuickMRC *me)
{
    if (me == NULL) {
        return;
    }
    qmrc__destroy(&me->qmrc);
    EvictingHashTable__destroy(&me->hash_table);
    Histogram__destroy(&me->histogram);
#ifdef INTERVAL_STATISTICS
    IntervalStatistics__destroy(&me->istats);
#endif
    *me = (struct EvictingQuickMRC){0};
}

bool
EvictingQuickMRC__get_histogram(struct EvictingQuickMRC const *const me,
                                struct Histogram const **const histogram)
{
    if (me == NULL || histogram == NULL) {
        return false;
    }
    *histogram = &me->histogram;
    return true;
}
