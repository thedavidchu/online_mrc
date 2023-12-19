/// @brief  This file abstracts the circularity of the buffer away from the
///         user. Instead, the user can view the buckets as an infinite array of
///         monotonically increasing buckets; as we increase the bucket index,
///         we simply move around the circle. However, the aging policy cannot
///         be implemented without knowledge of the circularity.
/// NOTE    Due to the complex interweaving between Mimir and its buckets data
///         structure, this file is essential solely for initializing and
///         destroying the bucket data structure in Mimir. In future, I may move
///         some of the functionality into here.

#pragma once

#include "logger/logger.h"
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h> // MAX()

// NOTE Please do not touch the internals of this struct unless you know what
//      you're doing! Otherwise, you could mess things up. I will still check
//      for errors. Or at least try to.
struct MimirBuckets {
    uint64_t *buckets;
    uint64_t num_buckets;
    // NOTE In Mimir's terminology, the newest bucket is that with the largest
    //      number. Conversely, the oldest is the one with the smallest.
    uint64_t newest_bucket;
    uint64_t oldest_bucket;
};

struct MimirBucketsStackDistanceStatus {
    bool success;
    uint64_t start;
    uint64_t range;
};

void
mimir_buckets__print_buckets(struct MimirBuckets *me);

bool
mimir_buckets__init(struct MimirBuckets *me, const uint64_t num_real_buckets)
{
    if (me == NULL || num_real_buckets == 0) {
        return false;
    }
    me->buckets = calloc(num_real_buckets, sizeof(*me->buckets));
    if (me->buckets == NULL) {
        return false;
    }
    me->num_buckets = num_real_buckets;
    me->newest_bucket = num_real_buckets - 1;
    me->oldest_bucket = 0;
    return true;
}

/// NOTE    This function CANNOT handle negative numbers (that underflow) in the
///         bucket_index parameter. If I wanted to support that, I would need to
///         add me->num_buckets until it would be in range.
static uint64_t
get_real_bucket_index(struct MimirBuckets *me, const uint64_t bucket_index)
{
    uint64_t real_index = 0;
    assert(me != NULL && me->num_buckets != 0 && "me should not be NULL");
    if (bucket_index < me->oldest_bucket) {
        real_index = me->oldest_bucket % me->num_buckets;
    } else {
        real_index = bucket_index % me->num_buckets;
    }
    return real_index;
}

/// @return Return 0 on error; otherwise, non-zero.
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
mimir_buckets__increment_newest_bucket(struct MimirBuckets *me)
{
    uint64_t real_index = 0;
    if (me == NULL || me->buckets == NULL) {
        return false;
    }
    real_index = get_real_bucket_index(me, me->newest_bucket);
    ++me->buckets[real_index];
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
    return true;
}

uint64_t
mimir_buckets__newest_bucket_size(struct MimirBuckets *me)
{
    uint64_t real_index = 0;
    if (me == NULL || me->buckets == NULL) {
        // NOTE This is an erroneous condition! I think returning zero is
        //      reasonable because if either of these is true, then all of the
        //      buckets are empty (according to my thinking...).
        return 0;
    }
    real_index = get_real_bucket_index(me, me->newest_bucket);
    return me->buckets[real_index];
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

/// @brief  Print the buckets in a quasi-JSON format. It's not JSON, though!
void
mimir_buckets__print_buckets(struct MimirBuckets *me)
{
    bool debug = false;
    bool values_only = true;
    if (me == NULL || me->buckets == NULL) {
        printf("(0, ?:?) []\n");
        return;
    }
    printf("(%" PRIu64 ", %" PRIu64 ":%" PRIu64 ") [",
           me->num_buckets,
           me->newest_bucket,
           me->oldest_bucket);
    if (debug) {
        for (uint64_t i = 0; i <= me->newest_bucket; ++i) {
            if (i < me->oldest_bucket) {
                printf("%" PRIu64 ": ?, ", i);
            } else {
                printf("%" PRIu64 ": %" PRIu64 ", ",
                       i,
                       me->buckets[i % me->num_buckets]);
            }
        }
    } else if (values_only) {
        for (uint64_t i = me->newest_bucket; i >= me->oldest_bucket; --i) {
            printf("%" PRIu64 ", ", me->buckets[i % me->num_buckets]);
        }
    } else {
        for (uint64_t i = me->newest_bucket; i >= me->oldest_bucket; --i) {
            printf("%" PRIu64 ": %" PRIu64 ", ",
                   i,
                   me->buckets[i % me->num_buckets]);
        }
    }
    printf("]\n");
}

bool
mimir_buckets__validate(struct MimirBuckets *me,
                        const uint64_t expected_max_num_unique_entries)
{
    if (me == NULL) {
        bool r = (expected_max_num_unique_entries == 0);
        assert(r);
        return r;
    }
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
    return actual_num_unique_entries == expected_max_num_unique_entries;
}

/// NOTE    You must not destroy an uninitialized MimirBuckets object lest
///         you free an uninitialized pointer (i.e. potentially non-NULL).
void
mimir_buckets__destroy(struct MimirBuckets *me)
{
    if (me == NULL) {
        return;
    }
    free(me->buckets);
    me->num_buckets = 0;
    // NOTE All other values don't really matter if we reset them...
    return;
}
