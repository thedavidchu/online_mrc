#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "types/value_type.h"
#include "types/key_type.h"
#include "types/time_stamp_type.h"

struct SampledHashTableNode;

struct SampledHashTable {
    struct SampledHashTableNode *data;
    size_t length;
};

enum SampledStatus {
    SAMPLED_NOTFOUND, 
    SAMPLED_NOTTRACKED,
    SAMPLED_FOUND,
    SAMPLED_REPLACED,
    SAMPLED_UPDATED,
};

struct SampledLookupReturn {
    enum SampledStatus status;
    TimeStampType timestamp;
};

bool
SampledHashTable__init(struct SampledHashTable *me, const size_t length);

struct SampledLookupReturn
SampledHashTable__lookup(struct SampledHashTable *me, KeyType key);

enum SampledStatus
SampledHashTable__put_unique(struct SampledHashTable *me, KeyType key, ValueType value);

void
SampledHashTable__destroy(struct SampledHashTable *me);
