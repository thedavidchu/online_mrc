#pragma once

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hash/MyMurmurHash3.h"
#include "hash/types.h"
#include "logger/logger.h"
#include "types/key_type.h"
#include "types/time_stamp_type.h"
#include "types/value_type.h"

struct EvictingHashTableNode {
    KeyType key;
    Hash64BitType hash;
    ValueType value;
};

struct EvictingHashTable {
    struct EvictingHashTableNode *data;
    size_t length;
    double init_sampling_ratio;
    Hash64BitType global_threshold;

    size_t num_inserted;
    double running_denominator;
    double hll_alpha_m;
};

enum SampledStatus {
    SAMPLED_HITHERTOEMPTY,
    SAMPLED_NOTFOUND,
    SAMPLED_IGNORED,
    SAMPLED_FOUND,
    SAMPLED_INSERTED,
    SAMPLED_REPLACED,
    SAMPLED_UPDATED,
};

struct SampledLookupReturn {
    enum SampledStatus status;
    Hash64BitType hash;
    TimeStampType timestamp;
};

struct SampledPutReturn {
    enum SampledStatus status;
    Hash64BitType new_hash;
    TimeStampType old_timestamp;
};

struct SampledTryPutReturn {
    enum SampledStatus status;
    Hash64BitType new_hash;
    KeyType old_key;
    Hash64BitType old_hash;
    ValueType old_value;
};

bool
EvictingHashTable__init(struct EvictingHashTable *me,
                        const size_t length,
                        const double init_sampling_ratio);

struct SampledLookupReturn
EvictingHashTable__lookup(struct EvictingHashTable *me, KeyType key);

struct SampledPutReturn
EvictingHashTable__put(struct EvictingHashTable *me,
                       KeyType key,
                       ValueType value);

/// @note   If we know the globally maximum threshold, then we can
///         immediately discard any element that is greater than this.
/// @note   This is an optimization to try to match SHARDS's performance.
///         Without this, we slightly underperform SHARDS. I don't know
///         how the Splay Tree priority queue is so fast...
void
EvictingHashTable__refresh_threshold(struct EvictingHashTable *me);

/// @brief  Try to put a value into the hash table.
/// @note   This combines the lookup and put traditionally used by the
///         MRC algorithm. I haven't thought too hard about whether all
///         other MRC algorithms could similarly combine the lookup and
///         put.
/// @note   Defining this as a `static inline` function improves performance
///         dramatically. In fact, without it, performance is much worse
///         than the separate lookup and put. I'm not exactly sure why, but
///         this has a much more complex return type.
struct SampledTryPutReturn
EvictingHashTable__try_put(struct EvictingHashTable *me,
                           KeyType key,
                           ValueType value);

double
EvictingHashTable__estimate_scale_factor(
    struct EvictingHashTable const *const me);

void
EvictingHashTable__print_as_json(struct EvictingHashTable *me);

void
EvictingHashTable__destroy(struct EvictingHashTable *me);
