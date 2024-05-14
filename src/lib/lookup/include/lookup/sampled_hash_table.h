#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hash/splitmix64.h"
#include "hash/types.h"
#include "types/key_type.h"
#include "types/time_stamp_type.h"
#include "types/value_type.h"

struct SampledHashTableNode {
    KeyType key;
    Hash64BitType hash;
    ValueType value;
};

struct SampledHashTable {
    struct SampledHashTableNode *data;
    size_t length;
    Hash64BitType global_threshold;

    size_t num_inserted;
    double running_denominator;
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
SampledHashTable__init(struct SampledHashTable *me,
                       const size_t length,
                       const double init_sampling_ratio);

struct SampledLookupReturn
SampledHashTable__lookup(struct SampledHashTable *me, KeyType key);

struct SampledPutReturn
SampledHashTable__put_unique(struct SampledHashTable *me,
                             KeyType key,
                             ValueType value);

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
SampledHashTable__try_put(struct SampledHashTable *me,
                          KeyType key,
                          ValueType value);

static inline struct SampledTryPutReturn
SampledHashTable__try_put(struct SampledHashTable *me,
                          KeyType key,
                          ValueType value)
{
    if (me == NULL || me->data == NULL || me->length == 0)
        return (struct SampledTryPutReturn){.status = SAMPLED_NOTFOUND};

    Hash64BitType hash = splitmix64_hash(key);
    if (hash > me->global_threshold)
        return (struct SampledTryPutReturn){.status = SAMPLED_IGNORED};

    struct SampledHashTableNode *incumbent = &me->data[hash % me->length];
    if (incumbent->hash == UINT64_MAX) {
        *incumbent = (struct SampledHashTableNode){.key = key,
                                                   .hash = hash,
                                                   .value = value};
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
        *incumbent = (struct SampledHashTableNode){.key = key,
                                                   .hash = hash,
                                                   .value = value};
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

/// @note   If we know the globally maximum threshold, then we can
///         immediately discard any element that is greater than this.
/// @note   This is an optimization to try to match SHARDS's performance.
///         Without this, we slightly underperform SHARDS. I don't know
///         how the Splay Tree priority queue is so fast...
void
SampledHashTable__refresh_threshold(struct SampledHashTable *me);

void
SampledHashTable__print_as_json(struct SampledHashTable *me);

void
SampledHashTable__destroy(struct SampledHashTable *me);
