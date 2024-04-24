#pragma once

#include <pthread.h>
#include <stdbool.h>

#include "lookup/lookup.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

struct ParallelList;
struct ParallelListNode;

struct ParallelList {
    pthread_rwlock_t lock;
    struct ParallelListNode *head;
    size_t length;
};

struct ParallelListNode {
    EntryType entry;
    TimeStampType timestamp;

    struct ParallelListNode *next;
};

bool
ParallelList__insert(struct ParallelList *me,
                     EntryType entry,
                     TimeStampType timestamp);

struct LookupReturn
ParallelList__lookup(struct ParallelList *me, EntryType entry);

bool
ParallelList__update(struct ParallelList *me,
                     EntryType entry,
                     TimeStampType timestamp);

/// Destroy the entire linked list pointed to by the head.
/// This may accept a NULL pointer.
void
ParallelList__destroy(struct ParallelList *me);

void
ParallelList__print(struct ParallelList *me);
