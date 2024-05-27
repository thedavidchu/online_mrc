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
EvictingHashTable__put_unique(struct EvictingHashTable *me,
                              KeyType key,
                              ValueType value);

/// @note   If we know the globally maximum threshold, then we can
///         immediately discard any element that is greater than this.
/// @note   This is an optimization to try to match SHARDS's performance.
///         Without this, we slightly underperform SHARDS. I don't know
///         how the Splay Tree priority queue is so fast...
void
EvictingHashTable__refresh_threshold(struct EvictingHashTable *me);

/// @brief  Count leading zeros in a uint64_t.
/// @note   The value of __builtin_clzll(0) is undefined, as per GCC's docs.
static inline int
clz(uint64_t x)
{
    return x == 0 ? 8 * sizeof(x) : __builtin_clzll(x);
}

/// @brief  Try to put a value into the hash table.
/// @note   This combines the lookup and put traditionally used by the
///         MRC algorithm. I haven't thought too hard about whether all
///         other MRC algorithms could similarly combine the lookup and
///         put.
/// @note   Defining this as a `static inline` function improves performance
///         dramatically. In fact, without it, performance is much worse than
///         the separate lookup and put. I'm not exactly sure why, but this has
///         a much more complex return type.
static inline struct SampledTryPutReturn
EvictingHashTable__try_put(struct EvictingHashTable *me,
                           KeyType key,
                           ValueType value);

static inline struct SampledTryPutReturn
EvictingHashTable__try_put(struct EvictingHashTable *me,
                           KeyType key,
                           ValueType value)
{
    if (me == NULL || me->data == NULL || me->length == 0)
        return (struct SampledTryPutReturn){.status = SAMPLED_NOTFOUND};

    Hash64BitType hash = Hash64bit(key);
    if (hash > me->global_threshold)
        return (struct SampledTryPutReturn){.status = SAMPLED_IGNORED};

    struct EvictingHashTableNode *incumbent = &me->data[hash % me->length];
    if (incumbent->hash == UINT64_MAX) {
        *incumbent = (struct EvictingHashTableNode){.key = key,
                                                    .hash = hash,
                                                    .value = value};
        ++me->num_inserted;
        if (me->num_inserted == me->length) {
            EvictingHashTable__refresh_threshold(me);
        }
        me->running_denominator +=
            exp2(-clz(hash) - 1) - 1 / me->init_sampling_ratio;
        return (struct SampledTryPutReturn){.status = SAMPLED_INSERTED,
                                            .new_hash = hash};
    }
    if (hash < incumbent->hash) {
        struct SampledTryPutReturn r = (struct SampledTryPutReturn){
            .status = SAMPLED_REPLACED,
            .new_hash = hash,
            .old_key = incumbent->key,
            .old_hash = incumbent->hash,
            .old_value = incumbent->value,
        };
        uint64_t const old_hash = incumbent->hash;
        // NOTE Update the incumbent before we do the scan for the maximum
        //      threshold because we want do not want to "find" that the
        //      maximum hasn't changed.
        *incumbent = (struct EvictingHashTableNode){.key = key,
                                                    .hash = hash,
                                                    .value = value};
        if (old_hash == me->global_threshold) {
            EvictingHashTable__refresh_threshold(me);
        }
        me->running_denominator +=
            exp2(-clz(hash) - 1) - exp2(-clz(old_hash) - 1);
        return r;
    }
    // NOTE If the key comparison is expensive, then one could first
    //      compare the hashes. However, in this case, they are not expensive.
    if (key == incumbent->key) {
        struct SampledTryPutReturn r = (struct SampledTryPutReturn){
            .status = SAMPLED_UPDATED,
            .new_hash = hash,
            .old_key = key,
            .old_hash = hash,
            .old_value = incumbent->value,
        };
        incumbent->value = value;
        return r;
    }
    return (struct SampledTryPutReturn){.status = SAMPLED_IGNORED};
}

double
EvictingHashTable__estimate_num_unique(struct EvictingHashTable *me);

void
EvictingHashTable__print_as_json(struct EvictingHashTable *me);

void
EvictingHashTable__destroy(struct EvictingHashTable *me);
