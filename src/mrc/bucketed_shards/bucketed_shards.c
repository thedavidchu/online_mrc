#include <assert.h>
#include <stdbool.h> // bool
#include <stdint.h>  // uint64_t
#include <stdio.h>
#include <stdlib.h>

#include "histogram/basic_histogram.h"
#include "lookup/lookup.h"
#include "lookup/sampled_hash_table.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "tree/types.h"
#include "types/time_stamp_type.h"

#include "bucketed_shards/bucketed_shards.h"

bool
BucketedShards__init(struct BucketedShards *me,
                     const uint64_t num_hash_buckets,
                     const uint64_t max_num_unique_entries)
{
    if (me == NULL)
        return false;
    bool r = tree__init(&me->tree);
    if (!r)
        goto tree_error;
    r = SampledHashTable__init(&me->hash_table, num_hash_buckets);
    if (!r)
        goto hash_table_error;
    r = BasicHistogram__init(&me->histogram, max_num_unique_entries);
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
    BasicHistogram__insert_finite(&me->histogram, distance);
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
    BasicHistogram__insert_infinite(&me->histogram);
}

void
BucketedShards__access_item(struct BucketedShards *me, EntryType entry)
{
    if (me == NULL)
        return;
    struct SampledLookupReturn found =
        SampledHashTable__lookup(&me->hash_table, entry);
    switch (found.status) {
    case SAMPLED_NOTTRACKED:
        /* Do no work -- this is like SHARDS */
        break;
    case SAMPLED_NOTFOUND:
        handle_not_found(me, entry);
        break;
    case SAMPLED_FOUND:
        handle_found(me, entry, found.timestamp);
        break;
    default:
        assert(0 && "impossible");
    }
}

void
BucketedShards__print_histogram_as_json(struct BucketedShards *me)
{
    if (me == NULL) {
        // Just pass on the NULL value and let the histogram deal with it. Maybe
        // this isn't very smart and will confuse future-me? Oh well!
        BasicHistogram__print_as_json(NULL);
        return;
    }
    BasicHistogram__print_as_json(&me->histogram);
}

void
BucketedShards__destroy(struct BucketedShards *me)
{
    if (me == NULL) {
        return;
    }
    tree__destroy(&me->tree);
    SampledHashTable__destroy(&me->hash_table);
    BasicHistogram__destroy(&me->histogram);
    *me = (struct BucketedShards){0};
}