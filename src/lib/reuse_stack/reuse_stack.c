#include <assert.h>
#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdio.h>
#include <stdlib.h>

#include "reuse_stack/reuse_stack.h"

gboolean
entry_compare(gconstpointer a, gconstpointer b)
{
    return (a == b) ? TRUE : FALSE;
}

struct ReuseStack *
reuse_stack_new()
{
    struct ReuseStack *reuse_stack = (struct ReuseStack *)malloc(sizeof(struct ReuseStack));
    if (reuse_stack == NULL) {
        return NULL;
    }
    reuse_stack->tree = tree_new();
    if (reuse_stack->tree == NULL) {
        reuse_stack_free(reuse_stack);
        return NULL;
    }
    // NOTE Using the g_direct_hash function means that we need to pass our
    //      entries as pointers to the hash table. It also means we cannot
    //      destroy the values at the pointers, because the pointers are our
    //      actual values!
    reuse_stack->hash_table = g_hash_table_new(g_direct_hash, entry_compare);
    if (reuse_stack->hash_table == NULL) {
        reuse_stack_free(reuse_stack);
        return NULL;
    }
    reuse_stack->histogram = (size_t *)calloc(MAX_HISTOGRAM_LENGTH, sizeof(size_t));
    if (reuse_stack->histogram == NULL) {
        reuse_stack_free(reuse_stack);
        return NULL;
    }
    reuse_stack->current_time_stamp = 0;
    reuse_stack->infinite_distance = 0;
    return reuse_stack;
}

void
reuse_stack_access_item(struct ReuseStack *me, EntryType entry)
{
    bool r = false;
    gboolean found = FALSE;
    TimeStampType time_stamp = 0;

    if (me == NULL) {
        return;
    }

    found = g_hash_table_lookup_extended(me->hash_table,
                                         (gconstpointer)entry,
                                         NULL,
                                         (gpointer *)&time_stamp);
    if (found == TRUE) {
        size_t distance = tree_reverse_rank(me->tree, (KeyType)time_stamp);
        r = sleator_remove(me->tree, (KeyType)time_stamp);
        assert(r && "remove should not fail");
        r = sleator_insert(me->tree, (KeyType)me->current_time_stamp);
        g_hash_table_replace(me->hash_table, (gpointer)entry, (gpointer)me->current_time_stamp);
        ++me->current_time_stamp;
        if (distance < MAX_HISTOGRAM_LENGTH) {
            ++me->histogram[distance];
        } else {
            ++me->infinite_distance;
            // TODO(dchu): Record the infinite distances for Parda!
        }
    } else {
        g_hash_table_insert(me->hash_table, (gpointer)entry, (gpointer)me->current_time_stamp);
        sleator_insert(me->tree, (KeyType)me->current_time_stamp);
        ++me->current_time_stamp;
        ++me->infinite_distance;
    }
}

void
reuse_stack_print_sparse_histogram(struct ReuseStack *me)
{
    if (me == NULL || me->histogram == NULL) {
        printf("{}\n");
        return;
    }
    printf("{");
    for (size_t i = 0; i < MAX_HISTOGRAM_LENGTH; ++i) {
        if (me->histogram[i]) {
            printf("\"%zu\": %zu, ", i, me->histogram[i]);
        }
    }
    printf("\"inf\": %zu", me->infinite_distance);
    printf("}\n");
}

void
reuse_stack_free(struct ReuseStack *me)
{
    if (me == NULL) {
        return;
    }
    tree_free(me->tree);
    // I do not know how the g_hash_table_destroy function behaves when passed a
    // NULL pointer. It is not in the documentation below:
    // https://docs.gtk.org/glib/type_func.HashTable.destroy.html
    if (me->hash_table != NULL) {
        // NOTE this doesn't destroy the key/values because they are just ints
        //      stored as pointers.
        g_hash_table_destroy(me->hash_table);
    }
    free(me->histogram);
}
