#include <assert.h>
#include <inttypes.h>
#include <stdbool.h> // bool
#include <stdint.h>  // uint64_t
#include <stdio.h>
#include <stdlib.h>

#include "mrc_reuse_stack/olken.h"

gboolean
entry_compare(gconstpointer a, gconstpointer b)
{
    return (a == b) ? TRUE : FALSE;
}

struct OlkenReuseStack *
olken_reuse_stack_new()
{
    struct OlkenReuseStack *olken_reuse_stack =
        (struct OlkenReuseStack *)malloc(sizeof(struct OlkenReuseStack));
    if (olken_reuse_stack == NULL) {
        return NULL;
    }
    olken_reuse_stack->tree = tree_new();
    if (olken_reuse_stack->tree == NULL) {
        olken_reuse_stack_free(olken_reuse_stack);
        return NULL;
    }
    // NOTE Using the g_direct_hash function means that we need to pass our
    //      entries as pointers to the hash table. It also means we cannot
    //      destroy the values at the pointers, because the pointers are our
    //      actual values!
    olken_reuse_stack->hash_table = g_hash_table_new(g_direct_hash, entry_compare);
    if (olken_reuse_stack->hash_table == NULL) {
        olken_reuse_stack_free(olken_reuse_stack);
        return NULL;
    }
    olken_reuse_stack->histogram = (uint64_t *)calloc(MAX_HISTOGRAM_LENGTH, sizeof(uint64_t));
    if (olken_reuse_stack->histogram == NULL) {
        olken_reuse_stack_free(olken_reuse_stack);
        return NULL;
    }
    olken_reuse_stack->current_time_stamp = 0;
    olken_reuse_stack->infinite_distance = 0;
    return olken_reuse_stack;
}

void
olken_reuse_stack_access_item(struct OlkenReuseStack *me, EntryType entry)
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
        uint64_t distance = tree_reverse_rank(me->tree, (KeyType)time_stamp);
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
olken_reuse_stack_print_sparse_histogram(struct OlkenReuseStack *me)
{
    if (me == NULL || me->histogram == NULL) {
        printf("{}\n");
        return;
    }
    printf("{");
    for (uint64_t i = 0; i < MAX_HISTOGRAM_LENGTH; ++i) {
        if (me->histogram[i]) {
            printf("\"" PRIu64 "\": " PRIu64 ", ", i, me->histogram[i]);
        }
    }
    printf("\"inf\": " PRIu64 "", me->infinite_distance);
    printf("}\n");
}

void
olken_reuse_stack_free(struct OlkenReuseStack *me)
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
