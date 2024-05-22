// NOTE Ashvin's code uses assert somewhere but doesn't include it in the
//      specific file, so I include it here as a catch all.
#include <assert.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "goel_quickmrc/goel_quickmrc.h"
#include "hash/MyMurmurHash3.h"
#include "logger/logger.h"
#include "math/ratio.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "types/entry_type.h"
#include "unused/mark_unused.h"

#ifndef QMRC
#error "QMRC must be defined here"
#endif

#include "quickmrc/histogram.h"

// HACK The structures are defined in the C files and moving them to the header
//      files would require a lot of stuff moved to the headers.
#include "qmrc.c"
#include "qmrc.h"
#include "quickmrc/cache.c"
#include "quickmrc/cache.h"

bool
GoelQuickMRC__init(struct GoelQuickMRC *me,
                   const double shards_sampling_ratio,
                   const int max_keys,
                   const int log_hist_buckets,
                   const int log_qmrc_buckets,
                   const int log_epoch_limit,
                   const bool shards_adjustment)
{
    if (me == NULL || shards_sampling_ratio <= 0.0 ||
        1.0 < shards_sampling_ratio)
        return false;

    int const log_max_keys = ceil(log2(max_keys));
    if (1 << log_max_keys < max_keys) {
        LOGGER_ERROR("log_max_keys = %d, max_keys = %d",
                     log_max_keys,
                     max_keys);
        LOGGER_ERROR("2^log_max_keys must be >= than max_keys");
        return false;
    }
    /* TODO: support larger max_keys. see definition of count_t in qmrc.c */
    if (log_max_keys < 0 || log_max_keys >= 32) {
        LOGGER_ERROR("log_max_keys = %d\n", log_max_keys);
        LOGGER_ERROR("log_max_keys must be between 0 and 2^32\n");
        return false;
    }
    if (log_hist_buckets <= 0) {
        LOGGER_ERROR("log_hist_buckets must be > 0\n");
        return false;
    }

#ifdef QMRC
    if (log_qmrc_buckets <= 0) {
        LOGGER_ERROR("log_qmrc_buckets must be > 0\n");
        return false;
    }
    if (log_qmrc_buckets > log_hist_buckets) {
        LOGGER_ERROR("log_qmrc_buckets must be <= log_hist_buckets\n");
        return false;
    }

    if (log_epoch_limit < 0 || log_epoch_limit >= 32) {
        LOGGER_ERROR("log_epoch_limit = %d\n", log_epoch_limit);
        LOGGER_ERROR("log_epoch_limit must be between 0 and 2^32\n");
        return false;
    }
    if (log_epoch_limit >= log_max_keys) {
        LOGGER_ERROR("log_max_keys = %d, log_epoch_limit = %d\n",
                     log_max_keys,
                     log_epoch_limit);
        LOGGER_ERROR("log_epoch_limit must be < log_max_keys\n");
        return false;
    }
#endif /* QMRC */
    me->sampling_ratio = shards_sampling_ratio;
    me->threshold = ratio_uint64(shards_sampling_ratio);
    me->scale = 1 / shards_sampling_ratio;
    me->shards_adjustment = shards_adjustment;
    me->num_entries_seen = 0;
    me->num_entries_processed = 0;
    me->cache = cache_init(log_max_keys,
                           log_hist_buckets,
                           log_qmrc_buckets,
                           log_epoch_limit);
    if (!me->cache)
        return false;
    return true;
}

bool
GoelQuickMRC__access_item(struct GoelQuickMRC *me, EntryType entry)
{
    // Okay, there is so much pointer chasing I'm not going to bother checking
    // for all NULL values. I'm going to assume that the cache_insert() function
    // checks for NULL values (which I know it doesn't).
    if (me == NULL)
        return false;
    ++me->num_entries_seen;
    if (Hash64bit(entry) > me->threshold)
        return true;
    ++me->num_entries_processed;
    cache_insert(me->cache, entry);
    return true;
}

void
GoelQuickMRC__post_process(struct GoelQuickMRC *me)
{
    if (me == NULL || me->cache == NULL || me->cache->qmrc == NULL) {
        LOGGER_TRACE("cannot post-process uninitialized structure");
        return;
    }

    // SHARDS-Adj seems to decrease the accuracy.
    if (!me->shards_adjustment) {
        LOGGER_TRACE("configured to skip the SHARDS adjustment");
        return;
    }

    // NOTE I need to scale the adjustment by the scale that I've been adjusting
    //      all values. Conversely, I could just not scale any values by the
    //      scale and I'd be equally well off (in fact, better probably,
    //      because a smaller chance of overflowing).
    const int64_t adjustment =
        me->scale *
        (me->num_entries_seen * me->sampling_ratio - me->num_entries_processed);
    // NOTE SHARDS-Adj only adds to the first bucket; but what if the
    //      adjustment would make it negative? Well, in that case, I
    //      add it to the next buckets. I figure this is OKAY because
    //      histogram bin size is configurable and it's like using a
    //      larger bin.
    int64_t tmp_adj = adjustment;
    for (size_t i = 0; i < me->cache->qmrc->hist.length; ++i) {
        int64_t hist = me->cache->qmrc->hist.hits[i];
        if ((int64_t)me->cache->qmrc->hist.hits[i] + tmp_adj < 0) {
            me->cache->qmrc->hist.hits[i] = 0;
            tmp_adj += hist;
        } else {
            me->cache->qmrc->hist.hits[i] += tmp_adj;
            break;
        }
    }
}

void
GoelQuickMRC__print_histogram_as_json(struct GoelQuickMRC *me)
{
    UNUSED(me);
}

bool
GoelQuickMRC__to_mrc(struct MissRateCurve *mrc, struct GoelQuickMRC *me)
{
    if (mrc == NULL || me == NULL || me->cache == NULL)
        return false;

    uint64_t const num_bins = me->cache->qmrc->hist.length;
    uint64_t const bin_size =
        (1 << me->cache->qmrc->hist.log_bucket_size) * me->scale;

    *mrc = (struct MissRateCurve){
        .miss_rate = calloc(num_bins + 1, sizeof(*mrc->miss_rate)),
        .bin_size = bin_size,
        .num_bins = num_bins};
    if (mrc->miss_rate == NULL) {
        LOGGER_ERROR("calloc failed");
        return false;
    }

    // Generate the MRC
    struct histogram hist = me->cache->qmrc->hist;
    const int64_t adjustment =
        me->scale *
        (me->num_entries_seen * me->sampling_ratio - me->num_entries_processed);
    const uint64_t total = me->num_entries_processed + adjustment;
    uint64_t tmp = total;
    for (uint64_t i = 0; i < num_bins; ++i) {
        const uint64_t h = hist.hits[i];
        // TODO(dchu): Check for division by zero! How do we intelligently
        //              resolve this?
        mrc->miss_rate[i] = (double)tmp / (double)total;
        // The subtraction should yield a non-negative result
        g_assert_cmpuint(tmp, >=, h);
        tmp -= h;
    }
    mrc->miss_rate[num_bins] = (double)tmp / (double)total;
    return true;
}

void
GoelQuickMRC__destroy(struct GoelQuickMRC *me)
{
    cache_destroy(me->cache);
    *me = (struct GoelQuickMRC){0};
}
