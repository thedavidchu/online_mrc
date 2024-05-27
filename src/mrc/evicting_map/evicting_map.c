#include <assert.h>
#include <stdbool.h> // bool
#include <stdint.h>  // uint64_t
#include <stdio.h>
#include <stdlib.h>

#include "hash/splitmix64.h"
#include "hash/types.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "lookup/evicting_hash_table.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "types/value_type.h"
#include "unused/mark_unused.h"

#include "evicting_map/evicting_map.h"

bool
BucketedShards__init(struct EvictingMap *me,
                     const double init_sampling_ratio,
                     const uint64_t num_hash_buckets,
                     const uint64_t histogram_num_bins,
                     const uint64_t histogram_bin_size)
{
    if (me == NULL)
        return false;
    bool r = tree__init(&me->tree);
    if (!r)
        goto tree_error;
    r = EvictingHashTable__init(&me->hash_table,
                                num_hash_buckets,
                                init_sampling_ratio);
    if (!r)
        goto hash_table_error;
    r = Histogram__init(&me->histogram, histogram_num_bins, histogram_bin_size);
    if (!r)
        goto histogram_error;
    me->current_time_stamp = 0;
    return true;

histogram_error:
    EvictingHashTable__destroy(&me->hash_table);
hash_table_error:
    tree__destroy(&me->tree);
tree_error:
    return false;
}

static inline void
handle_inserted(struct EvictingMap *me,
                struct SampledTryPutReturn s,
                TimeStampType value)
{
    UNUSED(s);
    assert(me != NULL);

    const uint64_t scale =
        EvictingHashTable__estimate_num_unique(&me->hash_table) /
        me->hash_table.length;
    bool r = false;
    MAYBE_UNUSED(r);

    r = tree__sleator_insert(&me->tree, value);
    assert(r);
    Histogram__insert_scaled_infinite(&me->histogram, scale == 0 ? 1 : scale);
    ++me->current_time_stamp;
}

static inline void
handle_replaced(struct EvictingMap *me,
                struct SampledTryPutReturn s,
                TimeStampType timestamp)
{
    assert(me != NULL);

    const uint64_t scale =
        EvictingHashTable__estimate_num_unique(&me->hash_table) /
        me->hash_table.length;
    bool r = false;
    MAYBE_UNUSED(r);

    r = tree__sleator_remove(&me->tree, s.old_value);
    assert(r);
    r = tree__sleator_insert(&me->tree, timestamp);
    assert(r);

    Histogram__insert_scaled_infinite(&me->histogram, scale == 0 ? 1 : scale);
    ++me->current_time_stamp;
}

static inline void
handle_updated(struct EvictingMap *me,
               struct SampledTryPutReturn s,
               TimeStampType timestamp)
{
    assert(me != NULL);

    const uint64_t scale =
        EvictingHashTable__estimate_num_unique(&me->hash_table) /
        me->hash_table.length;
    bool r = false;
    uint64_t distance = 0;
    MAYBE_UNUSED(r);

    distance = tree__reverse_rank(&me->tree, (KeyType)s.old_value);
    r = tree__sleator_remove(&me->tree, s.old_value);
    assert(r);
    r = tree__sleator_insert(&me->tree, timestamp);
    assert(r);

    Histogram__insert_scaled_finite(&me->histogram,
                                    distance,
                                    scale == 0 ? 1 : scale);
    ++me->current_time_stamp;
}

void
BucketedShards__access_item(struct EvictingMap *me, EntryType entry)
{
    if (me == NULL)
        return;

    ValueType timestamp = me->current_time_stamp;
    struct SampledTryPutReturn r =
        EvictingHashTable__try_put(&me->hash_table, entry, timestamp);

    switch (r.status) {
    case SAMPLED_IGNORED:
        /* Do no work -- this is like SHARDS */
        break;
    case SAMPLED_INSERTED:
        handle_inserted(me, r, timestamp);
        break;
    case SAMPLED_REPLACED:
        handle_replaced(me, r, timestamp);
        break;
    case SAMPLED_UPDATED:
        handle_updated(me, r, timestamp);
        break;
    default:
        assert(0 && "impossible");
    }
}

void
BucketedShards__refresh_threshold(struct EvictingMap *me)
{
    if (me == NULL)
        return;
    EvictingHashTable__refresh_threshold(&me->hash_table);
}

void
BucketedShards__print_histogram_as_json(struct EvictingMap *me)
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
BucketedShards__destroy(struct EvictingMap *me)
{
    if (me == NULL) {
        return;
    }
    tree__destroy(&me->tree);
    EvictingHashTable__destroy(&me->hash_table);
    Histogram__destroy(&me->histogram);
    *me = (struct EvictingMap){0};
}
