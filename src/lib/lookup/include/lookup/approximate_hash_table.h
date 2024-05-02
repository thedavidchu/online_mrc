#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lookup/lookup.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

struct SampledHashTableNode;

struct SampledHashTable {
    struct SampledHashTableNode *data;
    Hash64BitType hash;
    size_t length;
};

enum SampledStatus {
    SAMPLED_NOTFOUND, 
    SAMPLED_NOTTRACKED,
    SAMPLED_FOUND,
    SAMPLED_REPLACED,
    SAMPLED_INSERTED,
};

struct SampledLookupReturn {
    enum SampledStatus status;
    TimeStampType timestamp;
};

bool
SampledHashTable__init(struct SampledHashTable *me, const uint64_t length);

struct SampledLookupReturn
SampledHashTable__lookup(struct SampledHashTable *me, EntryType key);

enum SampledStatus
SampledHashTable__put_unique(struct SampledHashTable *me, EntryType key, TimeStampType value);

void
SampledHashTable__destroy(struct SampledHashTable *me);
