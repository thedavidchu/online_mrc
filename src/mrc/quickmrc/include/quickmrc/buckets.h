#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "types/time_stamp_type.h"

/// This structure is meant to count the number of entries whose timestamp falls
/// within a given range [min_timestamp, max_timestamp]. The min_timestamp is
/// implicitly provided by the next oldest bucket and is never required by our
/// algorithm.
struct TimestampRangeCount {
    // NOTE In principle, we only need the maximum timestamp (i.e. the newest
    //      time stamp. We can figure out the minimum timestamp by looking at
    //      the bucket that is older than this one.
    TimeStampType max_timestamp;
    uint64_t count;
};

struct QuickMRCBuckets {
    struct TimestampRangeCount *buckets;
    const uint64_t num_buckets;
    const uint64_t default_num_buckets;
    uint64_t max_bucket_size;
    uint64_t num_unique_entries;
    TimeStampType timestamp;
};

bool
QuickMRCBuckets__init(struct QuickMRCBuckets *me,
                       uint64_t default_num_buckets,
                       uint64_t max_bucket_size);

/// Increment the newest bucket and return
bool
QuickMRCBuckets__insert_new(struct QuickMRCBuckets *me);

/// Decrement a bucket corresponding to the old timestamp. Get the stack
/// distance.
/// @return Return stack distance or UINT64_MAX on error.
uint64_t
QuickMRCBuckets__reaccess_old(struct QuickMRCBuckets *me,
                               TimeStampType old_timestamp);

void
QuickMRCBuckets__print(struct QuickMRCBuckets *me);

void
QuickMRCBuckets__destroy(struct QuickMRCBuckets *me);
