#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "types/time_stamp_type.h"

#include "quickmrc/buckets.h"

bool
quickmrc_buckets__init(struct QuickMrcBuckets *me,
                       uint64_t default_num_buckets,
                       uint64_t max_bucket_size)
{
    if (me == NULL || default_num_buckets == 0) {
        return false;
    }
    void *buf = calloc(default_num_buckets, sizeof(*me->buckets));
    if (buf == NULL) {
        return false;
    }
    const struct QuickMrcBuckets tmp = (struct QuickMrcBuckets){
        .buckets = (struct TimestampRangeCount *)buf,
        .num_buckets = default_num_buckets,
        .default_num_buckets = default_num_buckets,
        .max_bucket_size = max_bucket_size,
        .newest_bucket = me->default_num_buckets - 1,
        .num_unique_entries = 0,
        .timestamp = 0,
    };
    memcpy(me, &tmp, sizeof(*me));
    return true;
}

static bool
is_newest_bucket_full(struct QuickMrcBuckets *me)
{
    assert(me != NULL && me->buckets != NULL);
    return me->buckets[me->newest_bucket].count >= me->max_bucket_size;
}

/// Age the MRC buckets
/// @todo   Make this visible to the hash table!
/// @note   We could also merge the first pair we encounter whose sum is below
///         some threshold. In this case, we could be slightly faster (if we're
///         searching from the right direction).
static bool
age(struct QuickMrcBuckets *me)
{
    assert(me != NULL && me->buckets != NULL);
    // Get pair with minimum sum. We return the index of the __ of the pair.
    // Given a tie, we will return the newer pair.
    uint64_t min_pair = 0;
    uint64_t min_pair_sum = 0;
    for (uint64_t i = 0; i < me->num_buckets - 1; ++i) {
        // We bias toward newer pairs so that there is less copying required.
        if (me->buckets[i].count + me->buckets[i + 1].count <= min_pair_sum) {
            min_pair = i;
        }
    }

    // Update range
    me->buckets[min_pair].count = min_pair_sum;
    me->buckets[min_pair].max_timestamp =
        me->buckets[min_pair + 1].max_timestamp;

    // Shift newer buckets to fill hole
    memmove(
        &me->buckets[min_pair + 1],
        // NOTE This may be out of bounds... well I think C allows you to
        //      reference one past the last element, which would be this one.
        // TODO(dchu): Lookup whether C allows you to reference
        //      one-past-the-last element.
        &me->buckets[min_pair + 2],
        (me->num_buckets - min_pair - 2) * sizeof(*me->buckets));
    // TODO(dchu):  reset the newest bucket to empty and make the timestamp
    //              larger by one. We also want to synchronize this with the
    //              timestamp that the hash table is using.
    me->buckets[me->newest_bucket].count = 0;
    ++me->timestamp;
    me->buckets[me->newest_bucket].max_timestamp = me->timestamp;
    return true;
}

/// Increment the newest bucket and return
TimeStampType
quickmrc_buckets__insert_new(struct QuickMrcBuckets *me)
{
    if (me == NULL) {
        return -1;
    }
    if (is_newest_bucket_full(me)) {
        bool r = age(me);
        if (!r) {
            return -1;
        }
    }
    ++me->buckets[me->newest_bucket].count;
    return me->newest_bucket;
}

static uint64_t
get_stack_distance_and_decrement(struct QuickMrcBuckets *me,
                                 TimeStampType old_timestamp)
{
    uint64_t stack_dist = 0;
    for (uint64_t i = 0; i < me->num_buckets; ++i) {
        uint64_t bucket_id = me->num_buckets - 1 - i;
        if (me->buckets[bucket_id].max_timestamp < old_timestamp) {
            --me->buckets[bucket_id].count;
            return stack_dist;
        }
        stack_dist += me->buckets[bucket_id].count;
    }
    --me->buckets[0].count;
    return stack_dist; /* Found in oldest bucket */
}

/// Decrement a bucket corresponding to the old timestamp. Get the stack
/// distance.
/// @return Return stack distance or UINT64_MAX on error.
uint64_t
quickmrc_buckets__reaccess_old(struct QuickMrcBuckets *me,
                               TimeStampType old_timestamp)
{
    if (me == NULL) {
        return UINT64_MAX;
    }

    uint64_t stack_dist = get_stack_distance_and_decrement(me, old_timestamp);
    bool r = quickmrc_buckets__insert_new(me);
    if (!r) {
        return UINT64_MAX;
    }
    return stack_dist;
}

void
quickmrc_buckets__destroy(struct QuickMrcBuckets *me)
{
    if (me == NULL) {
        return;
    }
    free(me->buckets);
    me->buckets = NULL;
    return;
}
