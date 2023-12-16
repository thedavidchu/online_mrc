#include <assert.h>
#include <inttypes.h>
#include <stdbool.h> // bool
#include <stdint.h>  // uint64_t
#include <stdio.h>
#include <stdlib.h>

#include "histogram/basic_histogram.h"
#include "olken/olken.h"
#include "tree/naive_tree.h"

static gboolean
entry_compare(gconstpointer a, gconstpointer b)
{
    return (a == b) ? TRUE : FALSE;
}

bool
olken___init(struct OlkenReuseStack *me, const uint64_t max_num_unique_entries)
{
    if (me == NULL) {
        return false;
    }
    bool r = tree__init(&me->tree);
    if (!r) {
        goto tree__error;
    }
    // NOTE Using the g_direct_hash function means that we need to pass our
    //      entries as pointers to the hash table. It also means we cannot
    //      destroy the values at the pointers, because the pointers are our
    //      actual values!
    me->hash_table = g_hash_table_new(g_direct_hash, entry_compare);
    if (me->hash_table == NULL) {
        goto hash_table_error;
    }
    r = basic_histogram__init(&me->histogram, max_num_unique_entries);
    if (!r) {
        goto histogram_error;
    }
    me->current_time_stamp = 0;
    return true;

histogram_error:
    g_hash_table_destroy(me->hash_table);
    me->hash_table = NULL;
hash_table_error:
    tree__destroy(&me->tree);
tree__error:
    return false;
}

void
olken___access_item(struct OlkenReuseStack *me, EntryType entry)
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
        uint64_t distance = tree__reverse_rank(&me->tree, (KeyType)time_stamp);
        r = tree_sleator_remove(&me->tree, (KeyType)time_stamp);
        assert(r && "remove should not fail");
        r = tree__sleator_insert(&me->tree, (KeyType)me->current_time_stamp);
        g_hash_table_replace(me->hash_table, (gpointer)entry, (gpointer)me->current_time_stamp);
        ++me->current_time_stamp;
        // TODO(dchu): Maybe record the infinite distances for Parda!
        basic_histogram__insert_finite(&me->histogram, distance);
    } else {
        g_hash_table_insert(me->hash_table, (gpointer)entry, (gpointer)me->current_time_stamp);
        tree__sleator_insert(&me->tree, (KeyType)me->current_time_stamp);
        ++me->current_time_stamp;
        basic_histogram__insert_infinite(&me->histogram);
    }
}

void
olken___print_sparse_histogram(struct OlkenReuseStack *me)
{
    if (me == NULL) {
        // Just pass on the NULL value and let the histogram deal with it. Maybe
        // this isn't very smart and will confuse future-me? Oh well!
        basic_histogram__print_sparse(NULL);
        return;
    }
    basic_histogram__print_sparse(&me->histogram);
}

void
olken___destroy(struct OlkenReuseStack *me)
{
    if (me == NULL) {
        return;
    }
    tree__destroy(&me->tree);
    // I do not know how the g_hash_table_destroy function behaves when passed a
    // NULL pointer. It is not in the documentation below:
    // https://docs.gtk.org/glib/type_func.HashTable.destroy.html
    if (me->hash_table != NULL) {
        // NOTE this doesn't destroy the key/values because they are just ints
        //      stored as pointers.
        g_hash_table_destroy(me->hash_table);
        me->hash_table = NULL;
    }
    basic_histogram__destroy(&me->histogram);
    me->current_time_stamp = 0;
}
