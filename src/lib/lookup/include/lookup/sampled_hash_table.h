#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hash/types.h"
#include "types/value_type.h"
#include "types/key_type.h"
#include "types/time_stamp_type.h"

struct SampledHashTableNode;

struct SampledHashTable {
    struct SampledHashTableNode *data;
    size_t length;
    Hash64BitType threshold;
};

enum SampledStatus {
    SAMPLED_NOTFOUND, 
    SAMPLED_NOTTRACKED,
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

bool
SampledHashTable__init(struct SampledHashTable *me, const size_t length, const double init_sampling_ratio);

struct SampledLookupReturn
SampledHashTable__lookup(struct SampledHashTable *me, KeyType key);

enum SampledStatus
SampledHashTable__put_unique(struct SampledHashTable *me, KeyType key, ValueType value);

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
