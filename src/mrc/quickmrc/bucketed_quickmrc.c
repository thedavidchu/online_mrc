#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <pthread.h>

#include "hash/MyMurmurHash3.h"
#include "histogram/histogram.h"
#include "lookup/sampled_hash_table.h"
#include "math/ratio.h"
#include "quickmrc/bucketed_quickmrc.h"
#include "quickmrc/buckets.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "unused/mark_unused.h"

bool
BucketedQuickMRC__init(struct BucketedQuickMRC *me,
                       const uint64_t default_num_buckets,
                       const uint64_t max_bucket_size,
                       const uint64_t histogram_length,
                       const double sampling_ratio,
                       const uint64_t max_size)
{
    bool r = false;
    if (me == NULL) {
        return false;
    }
    r = SampledHashTable__init(&me->hash_table, max_size, sampling_ratio);
    if (!r) {
        return false;
    }
    r = QuickMRCBuckets__init(&me->buckets,
                              default_num_buckets,
                              max_bucket_size);
    if (!r) {
        SampledHashTable__destroy(&me->hash_table);
        return false;
    }
    r = Histogram__init(&me->histogram, histogram_length, 1);
    if (!r) {
        SampledHashTable__destroy(&me->hash_table);
        QuickMRCBuckets__destroy(&me->buckets);
        return false;
    }
    me->total_entries_seen = 0;
    me->total_entries_processed = 0;
    me->threshold = ratio_uint64(sampling_ratio);
    me->scale = 1 / sampling_ratio;
    return true;
}

static inline uint64_t
get_epoch(struct BucketedQuickMRC *me)
{
    assert(me != NULL && me->buckets.buckets != NULL &&
           me->buckets.num_buckets > 0);
    TimeStampType epoch = me->buckets.buckets[0].max_timestamp;
    return epoch;
}

static inline void
handle_inserted(struct BucketedQuickMRC *me,
                struct SampledTryPutReturn s,
                TimeStampType value)
{
    UNUSED(s);
    UNUSED(value);
    assert(me != NULL);

    const uint64_t scale = 1; // TODO scale properly
    bool r = false;
    MAYBE_UNUSED(r);

    r = QuickMRCBuckets__insert_new(&me->buckets);
    assert(r);

    Histogram__insert_scaled_infinite(&me->histogram, scale);
}

static inline void
handle_replaced(struct BucketedQuickMRC *me,
                struct SampledTryPutReturn s,
                TimeStampType timestamp)
{
    UNUSED(timestamp);
    assert(me != NULL);

    const uint64_t scale = 1; // TODO scale properly
    bool r = false;
    MAYBE_UNUSED(r);

    // NOTE Technically, we are decrementing the old bucket with the victim
    //      element and incrementing the newest bucket with the new element.
    //      However, I'm lazy and do these together. And being sloppy, I add
    //      a nice little comment instead of just fixing my code.
    uint64_t stack_dist =
        QuickMRCBuckets__reaccess_old(&me->buckets, s.old_value);
    assert(stack_dist != UINT64_MAX && "reaccess should succeed!");
    MAYBE_UNUSED(stack_dist);

    Histogram__insert_scaled_infinite(&me->histogram, scale);
}

static inline void
handle_updated(struct BucketedQuickMRC *me,
               struct SampledTryPutReturn s,
               TimeStampType timestamp)
{
    UNUSED(timestamp);
    assert(me != NULL);

    const uint64_t scale = 1; // TODO scale properly

    uint64_t stack_dist =
        QuickMRCBuckets__reaccess_old(&me->buckets, s.old_value);
    assert(stack_dist != UINT64_MAX && "reaccess should succeed!");

    Histogram__insert_scaled_finite(&me->histogram, stack_dist, scale);
}

bool
BucketedQuickMRC__access_item(struct BucketedQuickMRC *me, EntryType entry)
{
    if (me == NULL) {
        return false;
    }

    if (Hash64bit(entry) > me->threshold)
        return true;

    // This assumes there won't be any errors further on.
    me->total_entries_processed += 1;

    ValueType timestamp = get_epoch(me);
    struct SampledTryPutReturn r =
        SampledHashTable__try_put(&me->hash_table, entry, timestamp);

    switch (r.status) {
    case SAMPLED_IGNORED:
        /* Do no work -- this is like SHARDS */
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
    return true;
}

void
BucketedQuickMRC__print_histogram_as_json(struct BucketedQuickMRC *me)
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
BucketedQuickMRC__destroy(struct BucketedQuickMRC *me)
{
    if (me == NULL) {
        return;
    }
    SampledHashTable__destroy(&me->hash_table);
    QuickMRCBuckets__destroy(&me->buckets);
    Histogram__destroy(&me->histogram);
    // The num_buckets is const qualified, so we do memset to sketchily avoid
    // a compiler error (at the expense of making this undefined behaviour).
    // ARGH, C IS SO FRUSTRATING SOMETIMES! I wish it had special provisions for
    // initializing and destroying the structures.
    memset(me, 0, sizeof(*me));
}
