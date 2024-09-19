#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <pthread.h>

#include "histogram/histogram.h"
#include "logger/logger.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "quickmrc/quickmrc.h"
#include "quickmrc/quickmrc_buckets.h"
#include "shards/fixed_rate_shards_sampler.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

bool
QuickMRC__init(struct QuickMRC *me,
               const double sampling_ratio,
               const uint64_t default_num_buckets,
               const uint64_t max_bucket_size,
               const uint64_t histogram_num_bins,
               const uint64_t histogram_bin_size)
{
    if (me == NULL) {
        goto cleanup;
    }
    if (!HashTable__init(&me->hash_table)) {
        goto cleanup;
    }
    if (!qmrc__init(&me->buckets, default_num_buckets, max_bucket_size)) {
        return false;
    }
    if (!Histogram__init(&me->histogram,
                         histogram_num_bins,
                         histogram_bin_size,
                         false)) {
        goto cleanup;
    }
    if (!FixedRateShardsSampler__init(&me->sampler, sampling_ratio, true)) {
        goto cleanup;
    }
    return true;
cleanup:
    QuickMRC__destroy(me);
    return false;
}

static bool
handle_update(struct QuickMRC *me, EntryType entry, TimeStampType timestamp)
{
    uint64_t stack_dist = qmrc__lookup(&me->buckets, timestamp);
    assert(stack_dist != UINT64_MAX);
    assert(me->buckets.epochs != NULL);
    TimeStampType new_timestamp = me->buckets.epochs[0];
    if (HashTable__put(&me->hash_table, entry, new_timestamp) !=
        LOOKUP_PUTUNIQUE_REPLACE_VALUE) {
        LOGGER_ERROR("unexpected put return");
        return false;
    }
    if (!Histogram__insert_scaled_finite(&me->histogram,
                                         stack_dist,
                                         me->sampler.scale)) {
        return false;
    }
    return true;
}

static bool
handle_insert(struct QuickMRC *me, EntryType entry)
{
    // NOTE This returns the current epoch number. These are different
    //      semantics than my own functions, but I'll leave it for now.
    qmrc__insert(&me->buckets);
    if (!Histogram__insert_scaled_infinite(&me->histogram, me->sampler.scale)) {
        LOGGER_DEBUG("histogram insertion failed");
        return false;
    }
    assert(me->buckets.epochs != NULL);
    TimeStampType new_timestamp = me->buckets.epochs[0];
    if (HashTable__put(&me->hash_table, entry, new_timestamp) !=
        LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE) {
        LOGGER_DEBUG("unexpected put return");
        return false;
    }
    return true;
}

bool
QuickMRC__access_item(struct QuickMRC *me, EntryType entry)
{
    if (me == NULL) {
        return false;
    }
    if (!FixedRateShardsSampler__sample(&me->sampler, entry)) {
        return true;
    }
    struct LookupReturn r = HashTable__lookup(&me->hash_table, entry);
    if (r.success) {
        if (!handle_update(me, entry, r.timestamp)) {
            return false;
        }
    } else {
        if (!handle_insert(me, entry)) {
            return false;
        }
    }
    return true;
}

void
QuickMRC__post_process(struct QuickMRC *me)
{
    if (me == NULL || me->histogram.histogram == NULL ||
        me->histogram.num_bins < 1) {
        return;
    }
    // TODO Have this function return a boolean value.
    FixedRateShardsSampler__post_process(&me->sampler, &me->histogram);
}

bool
QuickMRC__to_mrc(struct QuickMRC const *const me,
                 struct MissRateCurve *const mrc)
{
    return MissRateCurve__init_from_histogram(mrc, &me->histogram);
}

void
QuickMRC__print_histogram_as_json(struct QuickMRC const *const me)
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
    qmrc__destroy(&me->buckets);
    Histogram__destroy(&me->histogram);
    *me = (struct QuickMRC){0};
}
