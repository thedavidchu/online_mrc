#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <pthread.h>

#include "hash/hash.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "math/ratio.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "quickmrc/buckets.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

#include "lookup/hash_table.h"
#include "quickmrc/quickmrc.h"

bool
QuickMRC__init(struct QuickMRC *me,
               const double sampling_ratio,
               const uint64_t default_num_buckets,
               const uint64_t max_bucket_size,
               const uint64_t histogram_num_bins,
               const uint64_t histogram_bin_size)
{
    bool r = false;
    if (me == NULL) {
        return false;
    }
    r = HashTable__init(&me->hash_table);
    if (!r) {
        return false;
    }
    r = QuickMRCBuckets__init(&me->buckets,
                              default_num_buckets,
                              max_bucket_size);
    if (!r) {
        HashTable__destroy(&me->hash_table);
        return false;
    }
    r = Histogram__init(&me->histogram,
                        histogram_num_bins,
                        histogram_bin_size,
                        false);
    if (!r) {
        HashTable__destroy(&me->hash_table);
        QuickMRCBuckets__destroy(&me->buckets);
        return false;
    }
    me->total_entries_seen = 0;
    me->total_entries_processed = 0;
    me->sampling_ratio = sampling_ratio;
    me->threshold = ratio_uint64(sampling_ratio);
    me->scale = 1 / sampling_ratio;
    return true;
}

bool
QuickMRC__access_item(struct QuickMRC *me, EntryType entry)
{
    if (me == NULL) {
        return false;
    }

    ++me->total_entries_seen;
    if (Hash64Bit(entry) > me->threshold)
        return true;
    // This assumes there won't be any errors further on.
    ++me->total_entries_processed;

    struct LookupReturn r = HashTable__lookup(&me->hash_table, entry);
    if (r.success) {
        uint64_t stack_dist =
            QuickMRCBuckets__reaccess_old(&me->buckets, r.timestamp);
        if (stack_dist == UINT64_MAX) {
            return false;
        }
        TimeStampType new_timestamp = me->buckets.buckets[0].max_timestamp;
        HashTable__put(&me->hash_table, entry, new_timestamp);
        Histogram__insert_scaled_finite(&me->histogram, stack_dist, me->scale);
    } else {
        if (!QuickMRCBuckets__insert_new(&me->buckets)) {
            return false;
        }
        if (!Histogram__insert_scaled_infinite(&me->histogram, me->scale)) {
            return false;
        }
        TimeStampType new_timestamp = me->buckets.buckets[0].max_timestamp;
        HashTable__put(&me->hash_table, entry, new_timestamp);
    }

    return true;
}

void
QuickMRC__post_process(struct QuickMRC *me)
{
    if (me == NULL || me->histogram.histogram == NULL ||
        me->histogram.num_bins < 1)
        return;

    // NOTE SHARDS-Adj is part of the core algorithm, so I treat this as
    //      mandatory for everything except for the SHARD-less implementation.
    if (me->sampling_ratio == 1.0)
        return;

    // NOTE I need to scale the adjustment by the scale that I've been adjusting
    //      all values. Conversely, I could just not scale any values by the
    //      scale and I'd be equally well off (in fact, better probably,
    //      because a smaller chance of overflowing).
    const int64_t adjustment =
        me->scale * (me->total_entries_seen * me->sampling_ratio -
                     me->total_entries_processed);
    bool r = Histogram__adjust_first_buckets(&me->histogram, adjustment);
    if (!r) {
        LOGGER_WARN("error in adjusting buckets");
    }
}

bool
QuickMRC__to_mrc(struct QuickMRC const *const me,
                 struct MissRateCurve *const mrc)
{
    return MissRateCurve__init_from_histogram(mrc, &me->histogram);
}

void
QuickMRC__print_histogram_as_json(struct QuickMRC *me)
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
QuickMRC__destroy(struct QuickMRC *me)
{
    if (me == NULL) {
        return;
    }
    HashTable__destroy(&me->hash_table);
    QuickMRCBuckets__destroy(&me->buckets);
    Histogram__destroy(&me->histogram);
    // The num_buckets is const qualified, so we do memset to sketchily avoid
    // a compiler error (at the expense of making this undefined behaviour).
    // ARGH, C IS SO FRUSTRATING SOMETIMES! I wish it had special provisions for
    // initializing and destroying the structures.
    memset(me, 0, sizeof(*me));
}
