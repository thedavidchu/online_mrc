#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types/time_stamp_type.h"

#include "quickmrc/buckets.h"

bool
QuickMRCBuckets__init(struct QuickMRCBuckets *me,
                      uint64_t default_num_buckets,
                      uint64_t max_bucket_size)
{
    if (me == NULL || default_num_buckets == 0)
        return false;
    void *buf = calloc(default_num_buckets, sizeof(*me->buckets));
    if (buf == NULL)
        return false;
    const struct QuickMRCBuckets tmp = (struct QuickMRCBuckets){
        .buckets = (struct TimestampRangeCount *)buf,
        .num_buckets = default_num_buckets,
        .default_num_buckets = default_num_buckets,
        .max_bucket_size = max_bucket_size,
        .num_unique_entries = 0,
        .timestamp = 0,
    };
    memcpy(me, &tmp, sizeof(*me));
    return true;
}

static bool
is_newest_bucket_full(struct QuickMRCBuckets *me)
{
    assert(me != NULL && me->buckets != NULL);
    return me->buckets[0].count >= me->max_bucket_size;
}

/// Get pair with minimum sum. We return the index of the __ of the pair.
/// Given a tie, we will return the newer pair.
static uint64_t
get_min_bucket_pair(struct QuickMRCBuckets *me)
{
    assert(me != NULL && me->buckets != NULL);
    uint64_t min_pair = 0;
    uint64_t min_pair_sum = INT64_MAX;
    for (uint64_t i = 0; i < me->num_buckets - 1; ++i) {
        // We bias toward newer pairs so that there is less copying required.
        uint64_t current_pair_sum =
            me->buckets[i].count + me->buckets[i + 1].count;
        if (current_pair_sum <= min_pair_sum) {
            min_pair = i;
            min_pair_sum = current_pair_sum;
        }
    }
    return min_pair;
}

static uint64_t
get_sum_of_pair(struct QuickMRCBuckets *me, uint64_t pair_idx)
{
    assert(me != NULL && me->buckets != NULL && pair_idx + 1 < me->num_buckets);
    return me->buckets[pair_idx].count + me->buckets[pair_idx + 1].count;
}

/// Age the MRC buckets
/// @todo   Make this visible to the hash table!
/// @note   We could also merge the first pair we encounter whose sum is below
///         some threshold. In this case, we could be slightly faster (if we're
///         searching from the right direction).
static bool
age(struct QuickMRCBuckets *me)
{
    uint64_t min_pair = get_min_bucket_pair(me);
    // Update range
    me->buckets[min_pair] = (struct TimestampRangeCount){
        .count = get_sum_of_pair(me, min_pair),
        .max_timestamp = me->buckets[min_pair + 1].max_timestamp};

    if (me->buckets[min_pair].count > me->max_bucket_size) {
        me->max_bucket_size *= 2;
    }

    // Shift newer buckets to fill hole
    memmove(
        &me->buckets[1],
        // NOTE This may be out of bounds... well I think C allows you to
        //      reference one past the last element, which would be this one.
        // TODO(dchu): Lookup whether C allows you to reference
        //      one-past-the-last element.
        &me->buckets[0],
        (min_pair + 1) * sizeof(*me->buckets));
    // TODO(dchu):  reset the newest bucket to empty and make the timestamp
    //              larger by one. We also want to synchronize this with the
    //              timestamp that the hash table is using.
    ++me->timestamp;
    me->buckets[0] =
        (struct TimestampRangeCount){.count = 0,
                                     .max_timestamp = me->timestamp};
    return true;
}

static inline bool
increment_newest_bucket(struct QuickMRCBuckets *me)
{
    assert(me != NULL && me->buckets != NULL);
    ++me->buckets[0].count;
    if (is_newest_bucket_full(me)) {
        if (!age(me))
            return false;
    }
    return true;
}

/// Increment the newest bucket and return
bool
QuickMRCBuckets__insert_new(struct QuickMRCBuckets *me)
{
    if (me == NULL || me->buckets == NULL)
        return false;
    if (!increment_newest_bucket(me))
        return false;
    return true;
}

/// Get the stack distance of a timestamp and decrement that timestamp.
static uint64_t
get_stack_distance_and_decrement(struct QuickMRCBuckets *me,
                                 TimeStampType old_timestamp)
{
    uint64_t stack_dist = 0;
    assert(me != NULL && me->buckets != NULL);
    for (uint64_t i = 0; i < me->num_buckets; ++i) {
        stack_dist += me->buckets[i].count;
        if (me->buckets[i].max_timestamp == 0 ||
            me->buckets[i].max_timestamp < old_timestamp) {
            --me->buckets[i].count;
            assert(stack_dist > 0 && "stack_dist should be at least 1");
            return stack_dist - 1;
        }
    }
    --me->buckets[me->num_buckets - 1].count;
    assert(stack_dist > 0 && "stack_dist should be at least 1");
    return stack_dist - 1; /* Found in oldest bucket */
}

/// Decrement a bucket corresponding to the old timestamp. Get the stack
/// distance. Increment the newest bucket.
/// @return Return stack distance or UINT64_MAX on error.
uint64_t
QuickMRCBuckets__reaccess_old(struct QuickMRCBuckets *me,
                              TimeStampType old_timestamp)
{
    if (me == NULL)
        return UINT64_MAX;
    if (!increment_newest_bucket(me))
        return UINT64_MAX;
    uint64_t stack_dist = get_stack_distance_and_decrement(me, old_timestamp);
    return stack_dist;
}

bool
QuickMRCBuckets__decrement_old(struct QuickMRCBuckets *me,
                               TimeStampType old_timestamp)
{
    if (me == NULL || me->buckets == NULL || me->num_buckets == 0)
        return UINT64_MAX;
    uint64_t stack_dist = get_stack_distance_and_decrement(me, old_timestamp);
    return stack_dist;
}

void
QuickMRCBuckets__print(struct QuickMRCBuckets *me)
{
    printf("[");
    for (size_t i = 0; i < me->num_buckets; ++i) {
        printf("{%zu:%zu}, ",
               me->buckets[i].max_timestamp,
               me->buckets[i].count);
    }
    printf("]\n");
}

void
QuickMRCBuckets__destroy(struct QuickMRCBuckets *me)
{
    if (me == NULL)
        return;
    free(me->buckets);
    memset(me, 0, sizeof(*me));
    return;
}
