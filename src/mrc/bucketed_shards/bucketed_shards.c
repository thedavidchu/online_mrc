#include <assert.h>
#include <stdbool.h> // bool
#include <stdint.h>  // uint64_t
#include <stdio.h>
#include <stdlib.h>

#include "hash/splitmix64.h"
#include "hash/types.h"
#include "histogram/histogram.h"
#include "lookup/sampled_hash_table.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "types/time_stamp_type.h"

#include "bucketed_shards/bucketed_shards.h"

bool
BucketedShards__init(struct BucketedShards *me,
                     const uint64_t num_hash_buckets,
                     const uint64_t max_num_unique_entries,
                     const double init_sampling_ratio)
{
    if (me == NULL)
        return false;
    bool r = tree__init(&me->tree);
    if (!r)
        goto tree_error;
    r = SampledHashTable__init(&me->hash_table, num_hash_buckets, init_sampling_ratio);
    if (!r)
        goto hash_table_error;
    r = Histogram__init(&me->histogram, max_num_unique_entries, 1);
    if (!r)
        goto histogram_error;
    me->current_time_stamp = 0;
    return true;

histogram_error:
    SampledHashTable__destroy(&me->hash_table);
hash_table_error:
    tree__destroy(&me->tree);
tree_error:
    return false;
}

static void
handle_found(struct BucketedShards *me,
             EntryType entry,
             Hash64BitType hash,
             TimeStampType prev_timestamp)
{
    assert(me != NULL);
    uint64_t distance = tree__reverse_rank(&me->tree, (KeyType)prev_timestamp);
    bool r = tree__sleator_remove(&me->tree, (KeyType)prev_timestamp);
    assert(r && "remove should not fail");
    r = tree__sleator_insert(&me->tree, (KeyType)me->current_time_stamp);
    assert(r && "insert should not fail");
    enum SampledStatus s = SampledHashTable__put_unique(&me->hash_table,
                                                        entry,
                                                        me->current_time_stamp);
    assert(s == SAMPLED_UPDATED && "update should replace value");
    ++me->current_time_stamp;
    // TODO(dchu): Maybe record the infinite distances for Parda!
    Histogram__insert_scaled_finite(
        &me->histogram,
        distance,
        1 /*hash == 0 ? 1 : UINT64_MAX / hash*/);
}

static void
handle_not_found(struct BucketedShards *me, EntryType entry)
{
    assert(me != NULL);
    enum SampledStatus s = SampledHashTable__put_unique(&me->hash_table,
                                                        entry,
                                                        me->current_time_stamp);
    assert(s == SAMPLED_REPLACED && "update should insert key/value");
    tree__sleator_insert(&me->tree, (KeyType)me->current_time_stamp);
    ++me->current_time_stamp;

    Hash64BitType hash = splitmix64_hash(entry);
    Histogram__insert_scaled_infinite(
        &me->histogram,
        1 /*hash == 0 ? 1 : UINT64_MAX / hash*/);
}

void
BucketedShards__access_item(struct BucketedShards *me, EntryType entry)
{
    if (me == NULL)
        return;
    struct SampledLookupReturn found =
        SampledHashTable__lookup(&me->hash_table, entry);
#if 0
    if (found.status == SAMPLED_NOTTRACKED) {
    } else if (found.status == SAMPLED_NOTFOUND) {
        handle_not_found(me, entry);
    } else {
        handle_found(me, entry, found.hash, found.timestamp);
    }
#else
    switch (found.status) {
    case SAMPLED_NOTTRACKED:
        /* Do no work -- this is like SHARDS */
        break;
    case SAMPLED_NOTFOUND:
        handle_not_found(me, entry);
        break;
    case SAMPLED_FOUND:
        handle_found(me, entry, found.hash, found.timestamp);
        break;
    default:
        assert(0 && "impossible");
    }
#endif
}

void
BucketedShards__print_histogram_as_json(struct BucketedShards *me)
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
BucketedShards__destroy(struct BucketedShards *me)
{
    if (me == NULL) {
        return;
    }
    tree__destroy(&me->tree);
    SampledHashTable__destroy(&me->hash_table);
    Histogram__destroy(&me->histogram);
    *me = (struct BucketedShards){0};
}
