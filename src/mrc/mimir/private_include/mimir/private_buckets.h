/// NOTE    These are the private methods belonging to buckets.{c,h}. This file
///         is simply for the sake of unit testing. This may be a completely
///         terrible idea. We'll find out.
/// NOTE    Now, I'm wondering: do I even need private methods at all? It smells
///         of premature optimization to me...
/// NOTE    I'm not convinced this needs to be in a separate directory all for
///         the sake of hiding it. I think the name "private_buckets.h" is
///         enough of a hint that the file is supposed to be private. Already
///         done, but I know for the future.
#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "math/positive_ceiling_divide.h"
#include "mimir/buckets.h"

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

static uint64_t
get_newest_bucket_size(struct MimirBuckets *me)
{
    // NOTE This is an erroneous condition! I think returning zero is
    //      reasonable because if either of these is true, then all of the
    //      buckets are empty (according to my thinking...).
    assert(me != NULL && me->buckets != NULL);
    uint64_t real_index = 0;
    real_index = get_real_bucket_index(me, me->newest_bucket);
    return me->buckets[real_index];
}

static uint64_t
get_average_num_entries_per_bucket(struct MimirBuckets *me)
{
    // NOTE If we want to be rigourous with our returns, I think a return value
    //      of 0 would be reasonable.
    assert(me != NULL && me->buckets != NULL && me->num_buckets != 0);
    uint64_t avg_entries_per_bucket =
        POSITIVE_CEILING_DIVIDE(me->num_unique_entries, me->num_buckets);
    return avg_entries_per_bucket;
}

/// @brief  Count the weighted sum of indices, where weight refers to how many
///         elements the bucket contains.
static uint64_t
count_weighted_sum_of_bucket_indices(struct MimirBuckets *me)
{
    // NOTE This check is to avoid a division by zero. However, if this is true,
    //      then we haven't initialized the MimirBuckets properly!
    assert(me != NULL && me->buckets != NULL && me->num_buckets != 0);
    uint64_t sum_of_bucket_indices = 0;
    for (uint64_t i = me->oldest_bucket; i <= me->newest_bucket; ++i) {
        sum_of_bucket_indices += i * me->buckets[i % me->num_buckets];
    }
    return sum_of_bucket_indices;
}
