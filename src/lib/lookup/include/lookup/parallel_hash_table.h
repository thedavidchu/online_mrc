#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "lookup/lookup.h"
#include "lookup/parallel_list.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

struct ParallelHashTable {
    struct ParallelList *table;
    size_t length;
};

bool
ParallelHashTable__init(struct ParallelHashTable *me, size_t num_buckets);

bool
ParallelHashTable__put(struct ParallelHashTable *me,
                       EntryType entry,
                       TimeStampType timestamp);

bool
ParallelHashTable__put(struct ParallelHashTable *me,
                       EntryType entry,
                       TimeStampType new_timestamp);

struct LookupReturn
ParallelHashTable__lookup(struct ParallelHashTable *me, EntryType);

void
ParallelHashTable__destroy(struct ParallelHashTable *me);

void
ParallelHashTable__print(struct ParallelHashTable *me);
