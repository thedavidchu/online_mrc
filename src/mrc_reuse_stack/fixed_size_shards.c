#include <stdbool.h>

#include "hash/splitmix64.h"
#include "histogram/basic_histogram.h"
#include "tree/naive_tree.h"
#include "tree/sleator_tree.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

#include <glib.h>
#include <stdint.h>

static gboolean
entry_compare(gconstpointer a, gconstpointer b)
{
    return (a == b) ? TRUE : FALSE;
}

static void
make_room(struct FixedSizeShardsReuseStack *me)
{
    bool r = false;
    gboolean found = FALSE;
    EntryType entry = 0;
    TimeStampType time_stamp = 0;

    if (me == NULL || me->hash_table == NULL) {
        return;
    }
    Hash64BitType max_hash = splay_priority_queue_get_max_hash(&me->pq);
    while (true) {
        r = splay_priority_queue_remove(&me->pq, max_hash, &entry);
        if (!r) { // No more elements with the old max_hash
            // Update the new threshold and scale
            max_hash = splay_priority_queue_get_max_hash(&me->pq);
            me->threshold = max_hash;
            me->scale = UINT64_MAX / max_hash;
            break;
        }
        // Remove the entry/time-stamp from the hash table and tree
        found = g_hash_table_lookup_extended(me->hash_table,
                                             (gconstpointer)entry,
                                             NULL,
                                             (gpointer *)&time_stamp);
        assert(found == TRUE && "hash table should contain the entry");
        r = sleator_remove(&me->tree, (KeyType)time_stamp);
        assert(r && "remove should not fail");
        g_hash_table_remove(me->hash_table, (gconstpointer)entry);
    }
}

bool
fixed_size_shards_init(struct FixedSizeShardsReuseStack *me,
                       const uint64_t starting_scale,
                       const uint64_t max_size,
                       const uint64_t histogram_length)
{
    bool r = false;
    if (me == NULL || me->max_size == 0) {
        return false;
    }

    r = tree_init(&me->tree);
    if (!r) {
        return false;
    }

    me->hash_table = g_hash_table_new(g_direct_hash, entry_compare);
    if (olken_reuse_stack->hash_table == NULL) {
        tree_destroy(&me->tree);
        return false;
    }

    r = basic_histogram_init(&me->histogram, histogram_length);
    if (!r) {
        tree_destroy(&me->tree);
        g_hash_table_destroy(me->hash_table);
        return false;
    }

    r = splay_priority_queue_init(&me->pq, max_size);
    if (!r) {
        tree_destroy(&me->tree);
        g_hash_table_destroy(me->hash_table);
        basic_histogram_destroy(&me->histogram);
        return false;
    }
    me->current_time_stamp = 0;
    me->scale = starting_scale;
    me->threshold = UINT64_MAX / starting_scale;
    return true;
}

void
fixed_size_shards_access_item(struct FixedSizeShardsReuseStack *me, EntryType entry)
{
    bool r = false;
    gboolean found = FALSE;
    TimeStampType time_stamp = 0;

    // NOTE I use the nullness of the hash table as a proxy for whether this
    //      data structure has been initialized.
    if (me == NULL || me->hash_table == NULL) {
        return;
    }

    // Skip items above the threshold. Note that we accept items that are equal
    // to the threshold because the maximum hash is the threshold.
    if (splitmix64_hash((uint64_t)entry) > me->threshold) {
        return;
    }

    found = g_hash_table_lookup_extended(me->hash_table,
                                         (gconstpointer)entry,
                                         NULL,
                                         (gpointer *)&time_stamp);
    if (found == TRUE) {
        uint64_t distance = tree_reverse_rank(me->tree, (KeyType)time_stamp);
        r = sleator_remove(&me->tree, (KeyType)time_stamp);
        assert(r && "remove should not fail");
        r = sleator_insert(&me->tree, (KeyType)me->current_time_stamp);
        g_hash_table_replace(me->hash_table, (gpointer)entry, (gpointer)me->current_time_stamp);
        ++me->current_time_stamp;
        basic_histogram_insert_finite(&me->histogram, distance, me->scale);
    } else {
        if (splay_priority_queue_is_full(&me->pq)) {
            make_room(me);
        }
        splay_priority_queue_insert_if_room(&me->pq, splitmix64_hash((uint64_t)entry), entry);
        g_hash_table_insert(me->hash_table, (gpointer)entry, (gpointer)me->current_time_stamp);
        sleator_insert(me->tree, (KeyType)me->current_time_stamp);
        ++me->current_time_stamp;
        ++me->infinite_distance;
    }
}

void
fixed_size_shards_print_sparse_histogram(struct OlkenReuseStack *me)
{
    basic_histogram_print(&me->histogram);
}

void
fixed_size_shards_destroy(struct OlkenReuseStack *me)
{
    tree_destroy(&me->tree);
    g_hash_table_destroy(me->hash_table);
    basic_histogram_destroy(&me->histogram);
    splay_priority_queue_destroy(&me->pq);
    me->current_time_stamp = 0;
    me->threshold = 0;
    me->scale = 0;
}