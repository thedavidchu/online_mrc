#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <pthread.h>

#include "hash/MyMurmurHash3.h"
#include "histogram/histogram.h"
#include "math/ratio.h"
#include "quickmrc/buckets.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

#include "lookup/hash_table.h"
#include "quickmrc/quickmrc.h"

bool
QuickMRC__init(struct QuickMRC *me,
               const uint64_t default_num_buckets,
               const uint64_t max_bucket_size,
               const uint64_t histogram_length,
               const double sampling_ratio)
{
    bool r = false;
    if (me == NULL) {
        return false;
    }
    r = HashTable__init(&me->hash_table);
    if (!r) {
        return false;
    }
    r = QuickMRCBuckets__init(&me->buckets,
                              default_num_buckets,
                              max_bucket_size);
    if (!r) {
        HashTable__destroy(&me->hash_table);
        return false;
    }
    r = Histogram__init(&me->histogram, histogram_length, 1);
    if (!r) {
        HashTable__destroy(&me->hash_table);
        QuickMRCBuckets__destroy(&me->buckets);
        return false;
    }
    me->total_entries_seen = 0;
    me->total_entries_processed = 0;
    me->threshold = ratio_uint64(sampling_ratio);
    me->scale = 1 / sampling_ratio;
    return true;
}

bool
QuickMRC__access_item(struct QuickMRC *me, EntryType entry)
{
    if (me == NULL) {
        return false;
    }

    if (Hash64bit(entry) > me->threshold)
        return true;

    // This assumes there won't be any errors further on.
    me->total_entries_processed += 1;

    struct LookupReturn r = HashTable__lookup(&me->hash_table, entry);
    if (r.success) {
        uint64_t stack_dist =
            QuickMRCBuckets__reaccess_old(&me->buckets, r.timestamp);
        if (stack_dist == UINT64_MAX) {
            return false;
        }
        TimeStampType new_timestamp = me->buckets.buckets[0].max_timestamp;
        HashTable__put_unique(&me->hash_table, entry, new_timestamp);
        Histogram__insert_scaled_finite(&me->histogram, stack_dist, me->scale);
    } else {
        if (!QuickMRCBuckets__insert_new(&me->buckets)) {
            return false;
        }
        if (!Histogram__insert_scaled_infinite(&me->histogram, me->scale)) {
            return false;
        }
        TimeStampType new_timestamp = me->buckets.buckets[0].max_timestamp;
        HashTable__put_unique(&me->hash_table, entry, new_timestamp);
    }

    return true;
}

void
QuickMRC__print_histogram_as_json(struct QuickMRC *me)
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
QuickMRC__destroy(struct QuickMRC *me)
{
    if (me == NULL) {
        return;
    }
    HashTable__destroy(&me->hash_table);
    QuickMRCBuckets__destroy(&me->buckets);
    Histogram__destroy(&me->histogram);
    // The num_buckets is const qualified, so we do memset to sketchily avoid
    // a compiler error (at the expense of making this undefined behaviour).
    // ARGH, C IS SO FRUSTRATING SOMETIMES! I wish it had special provisions for
    // initializing and destroying the structures.
    memset(me, 0, sizeof(*me));
}
