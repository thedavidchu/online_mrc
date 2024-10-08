#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "histogram/histogram.h"
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

#include "profile/profile.h"

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
    if (!ProfileStatistics__init(&me->prof_stats_fast)) {
        goto cleanup;
    }
    if (!ProfileStatistics__init(&me->prof_stats_slow)) {
        goto cleanup;
    }
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

    const uint64_t scale = me->hash_table.scale_factor;
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

    const uint64_t scale = me->hash_table.scale_factor;
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

    const uint64_t scale = me->hash_table.scale_factor;
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
    uint64_t const start = start_tick_counter();
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
    struct SampledTryPutReturn r =
        EvictingHashTable__try_put(&me->hash_table, entry, timestamp);
    switch (r.status) {
    case SAMPLED_IGNORED:
        /* Do no work -- this is like SHARDS */
        handle_ignored(me, r, timestamp);
        UPDATE_PROFILE_STATISTICS(&me->prof_stats_fast, start);
        break;
    case SAMPLED_INSERTED:
        handle_inserted(me, r, timestamp);
        UPDATE_PROFILE_STATISTICS(&me->prof_stats_slow, start);
        break;
    case SAMPLED_REPLACED:
        handle_replaced(me, r, timestamp);
        UPDATE_PROFILE_STATISTICS(&me->prof_stats_slow, start);
        break;
    case SAMPLED_UPDATED:
        handle_updated(me, r, timestamp);
        UPDATE_PROFILE_STATISTICS(&me->prof_stats_slow, start);
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
    ProfileStatistics__log(&me->prof_stats_fast, "fast Evicting Map");
    ProfileStatistics__log(&me->prof_stats_slow, "slow Evicting Map");
    ProfileStatistics__destroy(&me->prof_stats_fast);
    ProfileStatistics__destroy(&me->prof_stats_slow);
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
