#include "lookup/parallel_list.h"
#include "lookup/lookup.h"
#include "types/entry_type.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static struct ParallelListNode *
ParallelListNode__create(EntryType entry, TimeStampType timestamp)
{
    struct ParallelListNode *node = calloc(1, sizeof(*node));
    if (node == NULL) {
        return NULL;
    }

    node->entry = entry;
    node->timestamp = timestamp;

    return node;
}

/**
 * @note Not synchronised.
 */
static struct ParallelListNode *
ParallelList__find_node(struct ParallelList *me, EntryType entry)
{
    if (me == NULL) {
        return NULL;
    }

    struct ParallelListNode *current = me->head;
    while (current != NULL) {
        if (current->entry == entry) {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

/**
 * @note Not synchronised.
 */
static struct ParallelListNode *
ParallelList__pop_node(struct ParallelList *me, EntryType entry)
{
    if (me == NULL) {
        return NULL;
    }

    for (struct ParallelListNode *prev = NULL, *current = me->head;
         current != NULL;
         prev = current, current = current->next) {
        if (current->entry == entry) {
            prev->next = current->next;
            return current;
        }
    }
    return NULL;
}

/** @brief  Update if the key exists, otherwise insert.
 *  @note   Splay the input.
 */
bool
ParallelList__put_unique(struct ParallelList *me,
                  EntryType entry,
                  TimeStampType timestamp)
{
    if (me == NULL) {
        return false;
    }

    pthread_rwlock_wrlock(&me->lock);

    struct ParallelListNode *node = NULL;
    node = ParallelList__pop_node(me, entry);
    if (node == NULL) {
        node = ParallelListNode__create(entry, timestamp);
        if (node == NULL) {
            pthread_rwlock_wrlock(&me->lock);
            return false;
        }
    } else {
        node->timestamp = timestamp;
    }

    node->next = me->head;
    me->head = node;
    ++me->length;
    pthread_rwlock_unlock(&me->lock);

    return true;
}

struct LookupReturn
ParallelList__lookup(struct ParallelList *me, EntryType entry)
{
    pthread_rwlock_rdlock(&me->lock);
    struct ParallelListNode *node = ParallelList__find_node(me, entry);
    pthread_rwlock_unlock(&me->lock);

    if (node == NULL) {
        return (struct LookupReturn){
            .success = false,
            .timestamp = 0,
        };
    }

    return (struct LookupReturn){
        .success = true,
        .timestamp = node->timestamp,
    };
}

static void
ParallelListNode__destroy(struct ParallelListNode *node)
{
    if (node == NULL) {
        return;
    }

    ParallelListNode__destroy(node->next);
    free(node);
}

/// Destroy the entire linked list pointed to by the head.
/// This may accept a NULL pointer.
void
ParallelList__destroy(struct ParallelList *me)
{
    if (me == NULL) {
        return;
    }

    ParallelListNode__destroy(me->head);
    me->head = NULL;
    pthread_rwlock_destroy(&me->lock);
}

void
ParallelList__print(struct ParallelList *me)
{
    if (me == NULL) {
        printf("null");
        return;
    }

    if (me->length == 0) {
        printf("[]\n");
        return;
    }

    printf("[\n");
    pthread_rwlock_rdlock(&me->lock);
    struct ParallelListNode *current = me->head;
    while (current != NULL) {
        printf("\t(%" PRIu64 ", %" PRIu64 "),\n",
               current->entry,
               current->timestamp);
        current = current->next;
    }
    pthread_rwlock_unlock(&me->lock);
    printf("]\n");
}
