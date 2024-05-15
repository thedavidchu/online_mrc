// NOTE Ashvin's code uses assert somewhere but doesn't include it in the
//      specific file, so I include it here as a catch all.
#include <assert.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "goel_quickmrc/goel_quickmrc.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "types/entry_type.h"
#include "unused/mark_unused.h"

#include "quickmrc/histogram.h"

// HACK The structures are defined in the C files and moving them to the header
//      files would require a lot of stuff moved to the headers.
#include "qmrc.c"
#include "qmrc.h"
#include "quickmrc/cache.c"
#include "quickmrc/cache.h"

bool
GoelQuickMRC__init(struct GoelQuickMRC *me,
                   const int log_max_keys,
                   const int log_hist_buckets,
                   const int log_qmrc_buckets,
                   const int log_epoch_limit,
                   const double shards_sampling_ratio)
{
    me->num_entries_seen = 0;
    me->cache = cache_init(log_max_keys,
                           log_hist_buckets,
                           log_qmrc_buckets,
                           log_epoch_limit);
    if (!me->cache)
        return false;
    UNUSED(shards_sampling_ratio);
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
    cache_insert(me->cache, entry);
    return true;
}

void
GoelQuickMRC__post_process(struct GoelQuickMRC *me)
{
    UNUSED(me);
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
    uint64_t const bin_size = 2 << me->cache->qmrc->hist.log_bucket_size;

    *mrc = (struct MissRateCurve){
        .miss_rate = calloc(num_bins * bin_size + 1, sizeof(*mrc->miss_rate)),
        .bin_size = bin_size,
        .num_bins = num_bins};
    if (mrc->miss_rate == NULL) {
        LOGGER_ERROR("calloc failed");
        return false;
    }

    // Generate the MRC
    struct histogram hist = me->cache->qmrc->hist;
    const uint64_t total = me->num_entries_seen;
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