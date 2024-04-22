#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "histogram/basic_histogram.h"
#include "quickmrc/buckets.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

#include "hash_table.h"
#include "quickmrc/quickmrc.h"

static gboolean
entry_compare(gconstpointer a, gconstpointer b)
{
    return (a == b) ? TRUE : FALSE;
}

bool
quickmrc__init(struct QuickMrc *me,
               uint64_t default_num_buckets,
               uint64_t max_bucket_size,
               uint64_t histogram_length)
{
    bool r = false;
    if (me == NULL) {
        return false;
    }
    me->hash_table = g_hash_table_new(g_direct_hash, entry_compare);
    if (me->hash_table == NULL) {
        return false;
    }
    r = quickmrc_buckets__init(&me->buckets,
                               default_num_buckets,
                               max_bucket_size);
    if (!r) {
        g_hash_table_destroy(me->hash_table);
        return false;
    }
    r = basic_histogram__init(&me->histogram, histogram_length);
    if (!r) {
        g_hash_table_destroy(me->hash_table);
        quickmrc_buckets__destroy(&me->buckets);
        return false;
    }
    me->total_entries_seen = 0;
    me->total_entries_processed = 0;
    return true;
}

bool
quickmrc__access_item(struct QuickMrc *me, EntryType entry)
{
    gboolean found = FALSE;
    TimeStampType timestamp = 0;
    if (me == NULL) {
        return false;
    }

    // This assumes there won't be any errors further on.
    ++me->total_entries_processed;

    found = g_hash_table_lookup_extended(me->hash_table,
                                         (gconstpointer)entry,
                                         NULL,
                                         (gpointer *)&timestamp);
    if (found == TRUE) {
        uint64_t stack_dist =
            quickmrc_buckets__reaccess_old(&me->buckets, timestamp);
        if (stack_dist == UINT64_MAX)
            return false;
        TimeStampType new_timestamp = me->buckets.buckets[0].max_timestamp;
        g_hash_table_replace(me->hash_table,
                             (gpointer)entry,
                             (gpointer)new_timestamp);
        basic_histogram__insert_finite(&me->histogram, stack_dist);
    } else {
        if (!quickmrc_buckets__insert_new(&me->buckets))
            return false;
        if (!basic_histogram__insert_infinite(&me->histogram))
            return false;
        TimeStampType new_timestamp = me->buckets.buckets[0].max_timestamp;
        g_hash_table_insert(me->hash_table,
                            (gpointer)entry,
                            (gpointer)new_timestamp);
    }
    return true;
}

void
quickmrc__print_histogram_as_json(struct QuickMrc *me)
{
    if (me == NULL) {
        // Just pass on the NULL value and let the histogram deal with it. Maybe
        // this isn't very smart and will confuse future-me? Oh well!
        basic_histogram__print_as_json(NULL);
        return;
    }
    basic_histogram__print_as_json(&me->histogram);
}

void
quickmrc__destroy(struct QuickMrc *me)
{
    if (me == NULL) {
        return;
    }
    g_hash_table_destroy(me->hash_table);
    quickmrc_buckets__destroy(&me->buckets);
    basic_histogram__destroy(&me->histogram);
    memset(me, 0, sizeof(*me));
}