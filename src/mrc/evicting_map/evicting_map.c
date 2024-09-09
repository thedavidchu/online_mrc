#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "histogram/histogram.h"
#include "logger/logger.h"
#ifdef INTERVAL_STATISTICS
#include "interval_statistics/interval_statistics.h"
#endif
#include "lookup/dictionary.h"
#include "lookup/evicting_hash_table.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "types/value_type.h"
#include "unused/mark_unused.h"

#include "evicting_map/evicting_map.h"

#ifdef THRESHOLD_STATISTICS
#include "statistics/statistics.h"
#endif

#ifdef PROFILE_STATISTICS
#include "profile/profile.h"
#endif

#define THRESHOLD_SAMPLING_PERIOD (1 << 20)

static bool
initialize(struct EvictingMap *const me,
           double const init_sampling_ratio,
           uint64_t const num_hash_buckets,
           uint64_t const histogram_num_bins,
           uint64_t const histogram_bin_size,
           enum HistogramOutOfBoundsMode const out_of_bounds_mode,
           struct Dictionary const *const dictionary)
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
                         out_of_bounds_mode))
        goto cleanup;
    me->dictionary = dictionary;
#ifdef INTERVAL_STATISTICS
    if (!IntervalStatistics__init(&me->istats, histogram_num_bins)) {
        goto cleanup;
    }
#endif
#ifdef THRESHOLD_STATISTICS
    if (!Statistics__init(&me->stats, 4)) {
        goto cleanup;
    }
#endif
#ifdef PROFILE_STATISTICS
    me->ticks_ht = 0;
    me->ticks_ignored = 0;
    me->ticks_inserted = 0;
    me->ticks_replaced = 0;
    me->ticks_updated = 0;
    me->cnt_ht = 0;
    me->cnt_ignored = 0;
    me->cnt_inserted = 0;
    me->cnt_replaced = 0;
    me->cnt_updated = 0;
#endif
    me->current_time_stamp = 0;
    return true;

cleanup:
    EvictingMap__destroy(me);
    return false;
}

bool
EvictingMap__init(struct EvictingMap *const me,
                  const double init_sampling_ratio,
                  const uint64_t num_hash_buckets,
                  const uint64_t histogram_num_bins,
                  const uint64_t histogram_bin_size)
{
    return initialize(me,
                      init_sampling_ratio,
                      num_hash_buckets,
                      histogram_num_bins,
                      histogram_bin_size,
                      HistogramOutOfBoundsMode__allow_overflow,
                      NULL);
}

bool
EvictingMap__init_full(struct EvictingMap *const me,
                       double const init_sampling_ratio,
                       uint64_t const num_hash_buckets,
                       uint64_t const histogram_num_bins,
                       uint64_t const histogram_bin_size,
                       enum HistogramOutOfBoundsMode const out_of_bounds_mode,
                       struct Dictionary const *const dictionary)
{
    return initialize(me,
                      init_sampling_ratio,
                      num_hash_buckets,
                      histogram_num_bins,
                      histogram_bin_size,
                      out_of_bounds_mode,
                      dictionary);
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
    IntervalStatistics__append_scaled(&me->istats,
                                      (double)distance,
                                      (double)(scale == 0 ? 1 : scale),
                                      (double)me->current_time_stamp -
                                          timestamp - 1);
#endif
    ++me->current_time_stamp;
}

bool
EvictingMap__access_item(struct EvictingMap *me, EntryType entry)
{
    if (me == NULL)
        return false;
    ValueType timestamp = me->current_time_stamp;
#ifdef THRESHOLD_STATISTICS
    if (timestamp % THRESHOLD_SAMPLING_PERIOD == 0) {
        size_t max_hash = 0, min_hash = SIZE_MAX;
        for (size_t i = 0; i < me->hash_table.length; ++i) {
            max_hash = MAX(max_hash, me->hash_table.hashes[i]);
            min_hash = MIN(min_hash, me->hash_table.hashes[i]);
        }
        uint64_t const stats[] = {timestamp,
                                  me->hash_table.global_threshold,
                                  max_hash,
                                  min_hash};
        Statistics__append_uint64(&me->stats, stats);
    }
#endif
#ifdef PROFILE_STATISTICS
    uint64_t start = 0;
#endif
#ifdef PROFILE_STATISTICS
    start = start_rdtsc();
#endif
    struct SampledTryPutReturn r =
        EvictingHashTable__try_put(&me->hash_table, entry, timestamp);
#ifdef PROFILE_STATISTICS
    me->ticks_ht += end_rdtsc(start);
    ++me->cnt_ht;
#endif
    switch (r.status) {
    case SAMPLED_IGNORED:
        /* Do no work -- this is like SHARDS */
#ifdef PROFILE_STATISTICS
        start = start_rdtsc();
#endif
        handle_ignored(me, r, timestamp);
#ifdef PROFILE_STATISTICS
        me->ticks_ignored += end_rdtsc(start);
        ++me->cnt_ignored;
#endif
        break;
    case SAMPLED_INSERTED:
#ifdef PROFILE_STATISTICS
        start = start_rdtsc();
#endif
        handle_inserted(me, r, timestamp);
#ifdef PROFILE_STATISTICS
        me->ticks_inserted += end_rdtsc(start);
        ++me->cnt_inserted;
#endif
        break;
    case SAMPLED_REPLACED:
#ifdef PROFILE_STATISTICS
        start = start_rdtsc();
#endif
        handle_replaced(me, r, timestamp);
#ifdef PROFILE_STATISTICS
        me->ticks_replaced += end_rdtsc(start);
        ++me->cnt_replaced;
#endif
        break;
    case SAMPLED_UPDATED:
#ifdef PROFILE_STATISTICS
        start = start_rdtsc();
#endif
        handle_updated(me, r, timestamp);
#ifdef PROFILE_STATISTICS
        me->ticks_updated += end_rdtsc(start);
        ++me->cnt_updated;
#endif
        break;
    default:
        assert(0 && "impossible");
    }
    return true;
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
#ifdef THRESHOLD_STATISTICS
    char const *stats_path = Dictionary__get(me->dictionary, "stats_path");
    if (stats_path == NULL) {
        stats_path = "Evicting-Map-stats.bin";
    }
    Statistics__save(&me->stats, stats_path);
    Statistics__destroy(&me->stats);
#endif
#ifdef PROFILE_STATISTICS
    LOGGER_INFO("profile statistics ticks -- Hash Table: %" PRIu64 "/%" PRIu64
                " | Ignored: %" PRIu64 "/%" PRIu64 " | Inserted: %" PRIu64
                "/%" PRIu64 " | Replaced: %" PRIu64 "/%" PRIu64
                " | Updated: %" PRIu64 "/%" PRIu64,
                me->ticks_ht,
                me->cnt_ht,
                me->ticks_ignored,
                me->cnt_ignored,
                me->ticks_inserted,
                me->cnt_inserted,
                me->ticks_replaced,
                me->cnt_replaced,
                me->ticks_updated,
                me->cnt_updated);
#endif
    *me = (struct EvictingMap){0};
}

bool
EvictingMap__get_histogram(struct EvictingMap const *const me,
                           struct Histogram const **const histogram)
{
    if (me == NULL || histogram == NULL) {
        return false;
    }
    *histogram = &me->histogram;
    return true;
}
