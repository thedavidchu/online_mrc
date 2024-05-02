#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <sys/types.h>

#include "histogram/fractional_histogram.h"
#include "logger/logger.h"
#include "math/positive_ceiling_divide.h"
#include "types/entry_type.h"

#include "mimir/buckets.h"
#include "mimir/mimir.h"
#include "unused/mark_unused.h"

static gboolean
entry_compare(gconstpointer a, gconstpointer b)
{
    return (a == b) ? TRUE : FALSE;
}

static void
stacker_aging_policy(struct Mimir *me)
{
    assert(me != NULL && me->hash_table != NULL);
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, me->hash_table);

    uint64_t entry = 0, bucket_index = 0;
    uint64_t average_stack_distance_bucket =
        MimirBuckets__get_average_bucket_index(&me->buckets);
    while (g_hash_table_iter_next(&iter,
                                  (gpointer *)&entry,
                                  (gpointer *)&bucket_index) == TRUE) {
        if (bucket_index >= average_stack_distance_bucket) {
            g_hash_table_iter_replace(&iter, (gpointer)(bucket_index - 1));
        }
    }
    // NOTE To optimize this repeated function call away, we could
    //      age the counts of each buckets (i.e. factor this
    //      function out of the loop). This is a future task, since
    //      I want to get the algorithm working as described.
    MimirBuckets__stacker_aging_policy(&me->buckets,
                                       average_stack_distance_bucket);
}

static void
age(struct Mimir *me)
{
    assert(me != NULL);
    switch (me->aging_policy) {
    case MIMIR_STACKER:
        stacker_aging_policy(me);
        break;
    case MIMIR_ROUNDER:
        MimirBuckets__rounder_aging_policy(&me->buckets);
        break;
    default:
        LOGGER_ERROR("aging policy should be MIMIR_STACKER or MIMIR_ROUNDER");
        assert(0 && "aging policy should be MIMIR_STACKER or MIMIR_ROUNDER!");
        exit(EXIT_FAILURE);
    }
    return;
}

static void
hit(struct Mimir *me, EntryType entry, uint64_t bucket_index)
{
    // Adjust the last bucket for the Rounder aging policy
    if (me->aging_policy == MIMIR_ROUNDER &&
        bucket_index < me->buckets.oldest_bucket) {
        bucket_index = me->buckets.oldest_bucket;
    }

    // Update hash table
    uint64_t newest_bucket =
        MimirBuckets__get_newest_bucket_index(&me->buckets);
    if (newest_bucket == 0) {
        LOGGER_FATAL("newest_bucket should be non-zero");
        assert(0 && "newest_bucket should be non-zero");
        exit(EXIT_FAILURE);
    }
    g_hash_table_replace(me->hash_table,
                         (gpointer)entry,
                         (gpointer)newest_bucket);
    // TODO(dchu): Maybe record the infinite distances for Parda!

    // Update histogram
    struct MimirBucketsStackDistanceStatus status =
        MimirBuckets__get_stack_distance(&me->buckets, bucket_index);
    if (status.success == false) {
        LOGGER_ERROR("status should be successful!");
        assert(0 && "impossible!");
        exit(0);
    }
    FractionalHistogram__insert_scaled_finite(&me->histogram,
                                              status.start,
                                              status.range,
                                              1);

    // Update the buckets
    MimirBuckets__decrement_bucket(&me->buckets, bucket_index);
    MimirBuckets__increment_newest_bucket(&me->buckets);
    if (MimirBuckets__newest_bucket_is_full(&me->buckets)) {
        age(me);
    }
}

static void
miss(struct Mimir *me, EntryType entry)
{
    // Update the hash table
    uint64_t newest_bucket =
        MimirBuckets__get_newest_bucket_index(&me->buckets);
    if (newest_bucket == 0) {
        LOGGER_FATAL("newest_bucket should be non-zero");
        assert(0 && "newest_bucket should be non-zero");
        exit(EXIT_FAILURE);
    }
    g_hash_table_insert(me->hash_table,
                        (gpointer)entry,
                        (gpointer)newest_bucket);

    // Update the histogram
    FractionalHistogram__insert_scaled_infinite(&me->histogram, 1);

    // Update the buckets
    MimirBuckets__increment_newest_bucket(&me->buckets);
    MimirBuckets__increment_num_unique_entries(&me->buckets);
    if (MimirBuckets__newest_bucket_is_full(&me->buckets)) {
        age(me);
    }
}

bool
Mimir__init(struct Mimir *me,
            const uint64_t num_buckets,
            const uint64_t bin_size,
            const uint64_t max_num_unique_entries,
            const enum MimirAgingPolicy aging_policy)
{
    bool r = false;
    if (me == NULL || num_buckets == 0) {
        return false;
    }
    r = MimirBuckets__init(&me->buckets, num_buckets);
    if (!r) {
        goto buckets_error;
    }
    r = FractionalHistogram__init(
        &me->histogram,
        POSITIVE_CEILING_DIVIDE(max_num_unique_entries, bin_size),
        bin_size);
    if (!r) {
        goto histogram_error;
    }
    // NOTE Using the g_direct_hash function means that we need to pass our
    //      entries as pointers to the hash table. It also means we cannot
    //      destroy the values at the pointers, because the pointers are our
    //      actual values!
    me->hash_table = g_hash_table_new(g_direct_hash, entry_compare);
    if (me->hash_table == NULL) {
        goto hash_table_error;
    }

    // Initialize other things
    me->aging_policy = aging_policy;
    return true;

// NOTE These are in reverse order so we perform the appropriate deconstruction
//      upon an error.
hash_table_error:
    FractionalHistogram__destroy(&me->histogram);
histogram_error:
    MimirBuckets__destroy(&me->buckets);
buckets_error:
    return false;
}

void
Mimir__access_item(struct Mimir *me, EntryType entry)
{
    gboolean found = FALSE;
    uint64_t bucket_index = 0;
    if (me == NULL) {
        return;
    }

    found = g_hash_table_lookup_extended(me->hash_table,
                                         (gconstpointer)entry,
                                         NULL,
                                         (gpointer *)&bucket_index);
    if (found == TRUE) {
        hit(me, entry, bucket_index);
    } else {
        miss(me, entry);
    }
}

static void
print_key_value(gpointer key, gpointer value, gpointer unused_user_data)
{
    EntryType entry = (EntryType)key;
    uint64_t bucket_index = (uint64_t)value;
    printf("\"%" PRIu64 "\": %" PRIu64 ", ", entry, bucket_index);
    UNUSED(unused_user_data);
}

void
Mimir__print_hash_table(struct Mimir *me)
{
    if (me == NULL) {
        printf("{\"type\": null}\n");
        return;
    }
    printf("{");
    g_hash_table_foreach(me->hash_table, print_key_value, NULL);
    printf("}\n");
}

void
Mimir__print_histogram_as_json(struct Mimir *me)
{
    if (me == NULL) {
        // Just pass on the NULL value and let the histogram deal with it. Maybe
        // this isn't very smart and will confuse future-me? Oh well!
        FractionalHistogram__print_as_json(NULL);
        return;
    }
    FractionalHistogram__print_as_json(&me->histogram);
}

bool
Mimir__validate(struct Mimir *me)
{
    bool r = false;
    if (me == NULL) {
        return true;
    }
    r = MimirBuckets__validate(&me->buckets);
    if (!r) {
        assert(0);
        return false;
    }
    if (me->buckets.num_unique_entries != me->histogram.infinity) {
        assert(0);
        return false;
    }
    if (me->buckets.num_unique_entries != g_hash_table_size(me->hash_table)) {
        assert(0);
        return false;
    }
    return true;
}

void
Mimir__destroy(struct Mimir *me)
{
    if (me == NULL) {
        return;
    }
    FractionalHistogram__destroy(&me->histogram);
    MimirBuckets__destroy(&me->buckets);
    if (me->hash_table != NULL) {
        g_hash_table_destroy(me->hash_table);
    }
    *me = (struct Mimir){0};
}
