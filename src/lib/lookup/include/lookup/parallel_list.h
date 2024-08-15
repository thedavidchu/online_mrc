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

/** @brief  Update if the key exists, otherwise insert.
 *  @note   Splay the input key to the front of the list.
 */
bool
ParallelList__put(struct ParallelList *me,
                  EntryType entry,
                  TimeStampType timestamp);

struct LookupReturn
ParallelList__lookup(struct ParallelList *me, EntryType entry);

/// @brief  Destroy the entire linked list pointed to by the head.
void
ParallelList__destroy(struct ParallelList *me);

void
ParallelList__print(struct ParallelList *me);
