#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "hash/splitmix64.h"
#include "histogram/histogram.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

#include "shards/fixed_size_shards.h"

static gboolean
entry_compare(gconstpointer a, gconstpointer b)
{
    return (a == b) ? TRUE : FALSE;
}

static void
make_room(struct FixedSizeShards *me)
{
    bool r = false;
    gboolean found = FALSE;
    EntryType entry = 0;
    TimeStampType time_stamp = 0;

    if (me == NULL || me->hash_table == NULL) {
        return;
    }
    Hash64BitType max_hash = SplayPriorityQueue__get_max_hash(&me->pq);
    while (true) {
        r = SplayPriorityQueue__remove(&me->pq, max_hash, &entry);
        if (!r) { // No more elements with the old max_hash
            // Update the new threshold and scale
            max_hash = SplayPriorityQueue__get_max_hash(&me->pq);
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
        r = tree__sleator_remove(&me->tree, (KeyType)time_stamp);
        assert(r && "remove should not fail");
        g_hash_table_remove(me->hash_table, (gconstpointer)entry);
    }
}

bool
FixedSizeShards__init(struct FixedSizeShards *me,
                      const double starting_sampling_ratio,
                      const uint64_t max_size,
                      const uint64_t max_num_unique_entries)
{
    bool r = false;
    if (me == NULL || starting_sampling_ratio <= 0.0 ||
        1.0 < starting_sampling_ratio || max_size == 0) {
        return false;
    }

    r = tree__init(&me->tree);
    if (!r) {
        return false;
    }

    me->hash_table = g_hash_table_new(g_direct_hash, entry_compare);
    if (me->hash_table == NULL) {
        tree__destroy(&me->tree);
        return false;
    }

    r = Histogram__init(&me->histogram, max_num_unique_entries, 1);
    if (!r) {
        tree__destroy(&me->tree);
        g_hash_table_destroy(me->hash_table);
        return false;
    }

    r = SplayPriorityQueue__init(&me->pq, max_size);
    if (!r) {
        tree__destroy(&me->tree);
        g_hash_table_destroy(me->hash_table);
        Histogram__destroy(&me->histogram);
        return false;
    }
    me->current_time_stamp = 0;
    me->scale = 1 / starting_sampling_ratio;
    // HACK We cast this to a `long double` because otherwise, the
    //      compiler does: ((double)UINT64_MAX), which rounds it up too
    //      high. This then causes the value to wrap around when we try
    //      to cast it back to a uint64_t.
    //
    // TODO(dchu)   Check other cases where we multiply UINT64_MAX by a
    //              double to make sure there can't be overflow.
    me->threshold = (long double)UINT64_MAX * starting_sampling_ratio;
    return true;
}

void
FixedSizeShards__access_item(struct FixedSizeShards *me, EntryType entry)
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
        uint64_t distance = tree__reverse_rank(&me->tree, (KeyType)time_stamp);
        r = tree__sleator_remove(&me->tree, (KeyType)time_stamp);
        assert(r && "remove should not fail");
        r = tree__sleator_insert(&me->tree, (KeyType)me->current_time_stamp);
        g_hash_table_replace(me->hash_table,
                             (gpointer)entry,
                             (gpointer)me->current_time_stamp);
        ++me->current_time_stamp;
        Histogram__insert_scaled_finite(&me->histogram,
                                              distance,
                                              me->scale);
    } else {
        if (SplayPriorityQueue__is_full(&me->pq)) {
            make_room(me);
        }
        SplayPriorityQueue__insert_if_room(&me->pq,
                                             splitmix64_hash((uint64_t)entry),
                                             entry);
        g_hash_table_insert(me->hash_table,
                            (gpointer)entry,
                            (gpointer)me->current_time_stamp);
        tree__sleator_insert(&me->tree, (KeyType)me->current_time_stamp);
        ++me->current_time_stamp;
        Histogram__insert_scaled_infinite(&me->histogram, me->scale);
    }
}

void
FixedSizeShards__print_histogram_as_json(struct FixedSizeShards *me)
{
    if (me == NULL) {
        // Just pass on the NULL value and let the histogram deal with it. Maybe
        // this isn't very smart and will confuse future-me? Oh well!
        Histogram__print_as_json(NULL);
        return;
    }
    Histogram__print_as_json(&me->histogram);
}

void
FixedSizeShards__destroy(struct FixedSizeShards *me)
{
    tree__destroy(&me->tree);
    g_hash_table_destroy(me->hash_table);
    Histogram__destroy(&me->histogram);
    SplayPriorityQueue__destroy(&me->pq);
    *me = (struct FixedSizeShards){0};
}
