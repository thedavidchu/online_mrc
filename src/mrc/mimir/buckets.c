
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h> // MAX()

#include "logger/logger.h"

#include "mimir/buckets.h"
#include "mimir/private_buckets.h"

bool
mimir_buckets__init(struct MimirBuckets *me, const uint64_t num_real_buckets)
{
    if (me == NULL || num_real_buckets == 0) {
        return false;
    }
    me->buckets = (uint64_t *)calloc(num_real_buckets, sizeof(*me->buckets));
    if (me->buckets == NULL) {
        return false;
    }
    me->num_buckets = num_real_buckets;
    me->newest_bucket = num_real_buckets - 1;
    me->oldest_bucket = 0;
    me->num_unique_entries = 0;
    me->sum_of_bucket_indices = 0;
    return true;
}

uint64_t
mimir_buckets__get_newest_bucket_index(struct MimirBuckets *me)
{
    if (me == NULL) {
        // Since we do not allow zero buckets, a value of zero would be an
        // invalid newest_bucket value.
        return 0;
    }
    return me->newest_bucket;
}

bool
mimir_buckets__increment_num_unique_entries(struct MimirBuckets *me)
{
    if (me == NULL) {
        return false;
    }
    ++me->num_unique_entries;
    return true;
}

bool
mimir_buckets__increment_newest_bucket(struct MimirBuckets *me)
{
    uint64_t real_index = 0;
    if (me == NULL || me->buckets == NULL) {
        return false;
    }
    real_index = get_real_bucket_index(me, me->newest_bucket);
    ++me->buckets[real_index];
    me->sum_of_bucket_indices += me->newest_bucket;
    return true;
}

bool
mimir_buckets__decrement_bucket(struct MimirBuckets *me,
                                const uint64_t bucket_index)
{
    uint64_t real_index = 0;
    if (me == NULL || me->buckets == NULL || bucket_index > me->newest_bucket) {
        return false;
    }

    real_index = get_real_bucket_index(me, bucket_index);
    --me->buckets[real_index];
    me->sum_of_bucket_indices -= bucket_index;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// GENERAL AGING POLICY
////////////////////////////////////////////////////////////////////////////////

bool
mimir_buckets__newest_bucket_is_full(struct MimirBuckets *me)
{
    if (me == NULL || me->num_buckets == 0) {
        return false;
    }
    uint64_t newest_bucket_size = get_newest_bucket_size(me);
    uint64_t avg_entries_per_bucket = get_average_num_entries_per_bucket(me);
    // This ceiling divide will return zero for a bucket array for which each
    // bucket is empty. If we find this to be problematic, we could either add 1
    // to the numerator or take the max of the numerator and 1 or I could do the
    // greater-than operator instead of the greater-than-or-equal. I did the
    // third option.
    bool debug = false;
    if (debug) {
        LOGGER_DEBUG("Newest bucket size: %" PRIu64
                     " vs average per bucket: %" PRIu64,
                     newest_bucket_size,
                     avg_entries_per_bucket);
    }
    if (newest_bucket_size > avg_entries_per_bucket) {
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
/// STACKER AGING POLICY
////////////////////////////////////////////////////////////////////////////////

uint64_t
mimir_buckets__get_average_bucket_index(struct MimirBuckets *me)
{
    if (me == NULL || me->num_unique_entries == 0) {
        return 0;
    }
    // We take the floor of the division because we want to include the bucket
    // that contains the average stack distance. I'm not convinced by this...
    // please provide an example!
    return me->sum_of_bucket_indices / me->num_unique_entries;
}

bool
mimir_buckets__stacker_aging_policy(struct MimirBuckets *me,
                                    const uint64_t average_bucket_index)
{
    if (me == NULL || me->buckets == NULL) {
        return false;
    }
    if (average_bucket_index <= me->oldest_bucket) {
        // This is only possible when me->num_unique_entries == 0 or when we are
        // aging at the improper time. Here is the "proof" (read: "spoof" lol):
        // 1. Assume we are aging at the proper time.
        //      => |newest_bucket| >= N/B
        //      => |oldest_bucket| <= N - N / B = (B - 1) * N / B
        // 2. Assume oldest_id >= 0.
        // 3. We know that newest_id = oldest_id + B - 1.
        //
        // => average_id    = newest_id * N / B + oldest_id * (B - 1) * N / B
        //                  = (oldest_id + B - 1) * N / B
        //                      + oldest_id * (B - 1) * N / B
        //                  = (oldest_id + B - 1 + oldest_id * B - oldest_id)
        //                      * N / B
        //                  = (B - 1 + oldest_id * B) * N / B
        //                  = (B - 1) * N / B + N * oldest_id
        // Now, is average_id = (B - 1) * N / B + N * oldest_id > oldest_id?
        //  => (B - 1) * N / B + N * oldest_id - oldest_id > 0
        //  => (B - 1) * N / B + (N - 1) * oldest_id > 0
        //  => if B > 1, then N simply has to be greater than 0.
        //
        // NOTE This would be more rigourous if I did all the integer rounding.
        assert(me->num_unique_entries == 0 &&
               "my 'proof'/'spoof' is incorrect!");
        printf("David was correct. Yet again.\n");
        return false;
    }
    assert(me->newest_bucket - average_bucket_index <= me->num_buckets - 1 &&
           "should be less than or equal to B - 1 distance");
    for (uint64_t i = average_bucket_index; i <= me->newest_bucket; ++i) {
        uint64_t new_real_index = get_real_bucket_index(me, i - 1);
        uint64_t old_real_index = get_real_bucket_index(me, i);
        me->buckets[new_real_index] += me->buckets[old_real_index];
        me->sum_of_bucket_indices -= me->buckets[old_real_index];
        me->buckets[old_real_index] = 0;
    }
    return true;
}

bool
mimir_buckets__age_by_one_bucket(struct MimirBuckets *me,
                                 const uint64_t bucket_index)
{
    uint64_t old_real_index = 0, new_real_index;
    if (me == NULL || me->buckets == NULL) {
        return false;
    }

    old_real_index = get_real_bucket_index(me, bucket_index);
    // NOTE Pre-emptively add me->num_buckets so that it is guaranteed to be
    //      positive.
    new_real_index =
        get_real_bucket_index(me, bucket_index + me->num_buckets - 1);
    --me->buckets[old_real_index];
    ++me->buckets[new_real_index];
    // We are aging the bucket by 1, which means that it moves to a lower
    // bucket.
    me->sum_of_bucket_indices -= 1;
    return true;
}

bool
mimir_buckets__rounder_aging_policy(struct MimirBuckets *me)
{
    uint64_t old_oldest_real_index = 0, new_oldest_real_index = 0;
    if (me == NULL || me->buckets == NULL) {
        return false;
    }

    old_oldest_real_index = get_real_bucket_index(me, me->oldest_bucket);
    new_oldest_real_index = get_real_bucket_index(me, me->oldest_bucket + 1);
    // All of the elements in the old-oldest bucket will be made newer by 1. We
    // do not use the me->sum_of_bucket_indices in the Rounder aging policy, but
    // I am doing this to better aid debugging.
    me->sum_of_bucket_indices += me->buckets[old_oldest_real_index];
    me->buckets[new_oldest_real_index] += me->buckets[old_oldest_real_index];
    me->buckets[old_oldest_real_index] = 0;
    ++me->oldest_bucket;
    ++me->newest_bucket;
    return true;
}

struct MimirBucketsStackDistanceStatus
mimir_buckets__get_stack_distance(struct MimirBuckets *me,
                                  uint64_t bucket_index)
{
    struct MimirBucketsStackDistanceStatus status = {.success = false};
    if (me == NULL || me->buckets == NULL || bucket_index > me->newest_bucket) {
        return status;
    }

    // NOTE According to how I currently call this function, this is redundant.
    //      I do this because the minimum allowable value for the bucket index
    //      is the me->oldest_bucket.
    bucket_index = MAX(bucket_index, me->oldest_bucket);
    status.start = 0;
    // Iterate from one-past the resident bucket (i.e. time_stamp) and the
    // newest bucket.
    for (uint64_t i = bucket_index + 1; i <= me->newest_bucket; ++i) {
        status.start += me->buckets[i % me->num_buckets];
    }
    status.range = me->buckets[bucket_index % me->num_buckets];
    status.success = true;
    return status;
}

void
mimir_buckets__print_buckets(struct MimirBuckets *me,
                             enum MimirBucketsPrintMode mode)
{
    if (me == NULL || me->buckets == NULL) {
        printf("(0, ?:?) []\n");
        return;
    }
    printf("(%" PRIu64 ", %" PRIu64 ":%" PRIu64 ") [",
           me->num_buckets,
           me->newest_bucket,
           me->oldest_bucket);
    switch (mode) {
    case MIMIR_BUCKETS_PRINT_DEBUG:
        // NOTE This prints oldest first, then newest
        for (uint64_t i = 0; i <= me->newest_bucket; ++i) {
            if (i < me->oldest_bucket) {
                printf("%" PRIu64 ": ?, ", i);
            } else {
                printf("%" PRIu64 ": %" PRIu64 ", ",
                       i,
                       me->buckets[i % me->num_buckets]);
            }
        }
        break;
    case MIMIR_BUCKETS_PRINT_KEYS_AND_VALUES:
        // NOTE This prints newest first, then oldest
        for (uint64_t i = 0; i < me->num_buckets; ++i) {
            const uint64_t b_idx = me->newest_bucket - i;
            printf("%" PRIu64 ": %" PRIu64 ", ",
                   b_idx,
                   me->buckets[b_idx % me->num_buckets]);
        }
        break;
    case MIMIR_BUCKETS_PRINT_VALUES_ONLY: /* Intentional fallthrough */
    default:
        // NOTE This prints newest first, then oldest
        for (uint64_t i = 0; i < me->num_buckets; ++i) {
            const uint64_t b_idx = me->newest_bucket - i;
            printf("%" PRIu64 ", ", me->buckets[b_idx % me->num_buckets]);
        }
        break;
    }
    printf("]\n");
}

bool
mimir_buckets__validate(struct MimirBuckets *me)
{
    if (me == NULL) {
        return true;
    }
    const uint64_t expected_max_num_unique_entries = me->num_unique_entries;
    if (me->buckets == NULL) {
        bool r = (expected_max_num_unique_entries == 0 && me->num_buckets == 0);
        assert(r);
        return r;
    }
    if (me->newest_bucket + 1 - me->num_buckets != me->oldest_bucket) {
        assert(0);
        return false;
    }
    uint64_t actual_num_unique_entries = 0;
    for (uint64_t i = 0; i < me->num_buckets; ++i) {
        uint64_t num = me->buckets[i];
        if ((int64_t)num < 0) {
            LOGGER_WARN("bucket with real number %" PRIu64
                        " may be very large (%" PRIu64
                        "), but it may have also underflowed (%" PRId64 ")",
                        i,
                        num,
                        (int64_t)num);
        }
        actual_num_unique_entries += me->buckets[i];
    }
    if (actual_num_unique_entries != expected_max_num_unique_entries) {
        assert(0);
        return false;
    }
    if (me->sum_of_bucket_indices != count_weighted_sum_of_bucket_indices(me)) {
        assert(0);
        return false;
    }
    return true;
}

void
mimir_buckets__destroy(struct MimirBuckets *me)
{
    if (me == NULL) {
        return;
    }
    free(me->buckets);
    me->num_buckets = 0;
    // I don't really have to do this, but I do so just to be safe. Actually, I
    // don't have a consistent way of measuring whether a data structure has
    // been initialized (or deinitialized) yet. Also, side note: should I change
    // function names from *__destroy() to *__deinit()?
    me->num_unique_entries = 0;
    me->sum_of_bucket_indices = 0;
    // NOTE All other values don't really matter if we reset them...
    return;
}
