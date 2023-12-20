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

#include <stdbool.h>
#include <stdint.h>

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

    // This is the number of entries that are in the Mimir buckets. I maintain
    // this field because I need to know when to perform the aging policy.
    uint64_t num_unique_entries;
    // The weighted-sum of the bucket indices. By weighted, I mean that buckets
    // with more entries (i.e. buckets[i] is bigger) will be weighted more.
    uint64_t sum_of_bucket_indices;
};

struct MimirBucketsStackDistanceStatus {
    bool success;
    uint64_t start;
    uint64_t range;
};

enum MimirBucketsPrintMode {
    MIMIR_BUCKETS_PRINT_DEBUG,
    MIMIR_BUCKETS_PRINT_KEYS_AND_VALUES,
    MIMIR_BUCKETS_PRINT_VALUES_ONLY,
};

bool
mimir_buckets__init(struct MimirBuckets *me, const uint64_t num_real_buckets);

/// @return Return 0 on error; otherwise, non-zero.
uint64_t
mimir_buckets__get_newest_bucket_index(struct MimirBuckets *me);

bool
mimir_buckets__increment_num_unique_entries(struct MimirBuckets *me);

bool
mimir_buckets__increment_newest_bucket(struct MimirBuckets *me);

bool
mimir_buckets__decrement_bucket(struct MimirBuckets *me,
                                const uint64_t bucket_index);

////////////////////////////////////////////////////////////////////////////////
/// GENERAL AGING POLICY
////////////////////////////////////////////////////////////////////////////////

uint64_t
mimir_buckets__newest_bucket_size(struct MimirBuckets *me);

/// @brief  Return whether the newest bucket has more than its fair share of
///         elements (defined to be greater than the average).
/// NOTE    This is a separate function because in the Mimir paper, it is
///         unclear whether this should be a ceiling-divide. In some places it
///         is; in others, it isn't. I use a ceiling-divide because it makes
///         more sense to me.
bool
mimir_buckets__newest_bucket_is_full(struct MimirBuckets *me);

////////////////////////////////////////////////////////////////////////////////
/// STACKER AGING POLICY
////////////////////////////////////////////////////////////////////////////////

uint64_t
mimir_buckets__count_sum_of_bucket_indices(struct MimirBuckets *me);

uint64_t
mimir_buckets__get_average_bucket(struct MimirBuckets *me);

bool
mimir_buckets__age_by_one_bucket(struct MimirBuckets *me,
                                 const uint64_t bucket_index);

bool
mimir_buckets__rounder_aging_policy(struct MimirBuckets *me);

struct MimirBucketsStackDistanceStatus
mimir_buckets__get_stack_distance(struct MimirBuckets *me,
                                  uint64_t bucket_index);

/// @brief  Print the buckets in a quasi-JSON format. It's not JSON, though!
void
mimir_buckets__print_buckets(struct MimirBuckets *me,
                             enum MimirBucketsPrintMode mode);

bool
mimir_buckets__validate(struct MimirBuckets *me);

/// NOTE    You must not destroy an uninitialized MimirBuckets object lest
///         you free an uninitialized pointer (i.e. potentially non-NULL).
void
mimir_buckets__destroy(struct MimirBuckets *me);
