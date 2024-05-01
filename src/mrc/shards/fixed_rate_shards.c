#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "hash/splitmix64.h"
#include "hash/types.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "olken/olken.h"
#include "shards/fixed_rate_shards.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "types/entry_type.h"

bool
FixedRateShards__init(struct FixedRateShards *me,
                      const uint64_t max_num_unique_entries,
                      const double sampling_ratio)
{
    if (me == NULL || sampling_ratio <= 0.0 || 1.0 < sampling_ratio)
        return false;
    // NOTE I am assuming that Olken does not have any structures that
    //      point to the containing structure (i.e. the 'shell' of the
    //      Olken structure is not referenced anywhere).
    bool r = Olken__init(&me->olken, max_num_unique_entries);
    if (!r)
        return false;
    me->sampling_ratio = sampling_ratio;
    return true;
}

void
FixedRateShards__access_item(struct FixedRateShards *me, EntryType entry)
{
    bool r = false;

    if (me == NULL) {
        return;
    }

    Hash64BitType hash = splitmix64_hash(entry);
    if (hash > UINT64_MAX * me->sampling_ratio)
        return;

    struct LookupReturn found = HashTable__lookup(&me->olken.hash_table, entry);
    if (found.success) {
        uint64_t distance =
            tree__reverse_rank(&me->olken.tree, (KeyType)found.timestamp);
        r = tree__sleator_remove(&me->olken.tree, (KeyType)found.timestamp);
        assert(r && "remove should not fail");
        r = tree__sleator_insert(&me->olken.tree,
                                 (KeyType)me->olken.current_time_stamp);
        assert(r && "insert should not fail");
        enum PutUniqueStatus s =
            HashTable__put_unique(&me->olken.hash_table,
                                  entry,
                                  me->olken.current_time_stamp);
        assert(s == LOOKUP_PUTUNIQUE_REPLACE_VALUE &&
               "update should replace value");
        ++me->olken.current_time_stamp;
        // TODO(dchu): Maybe record the infinite distances for Parda!
        basic_histogram__insert_scaled_finite(&me->olken.histogram, distance, 1 / me->sampling_ratio);
    } else {
        enum PutUniqueStatus s =
            HashTable__put_unique(&me->olken.hash_table,
                                  entry,
                                  me->olken.current_time_stamp);
        assert(s == LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE &&
               "update should insert key/value");
        tree__sleator_insert(&me->olken.tree,
                             (KeyType)me->olken.current_time_stamp);
        ++me->olken.current_time_stamp;
        basic_histogram__insert_scaled_infinite(&me->olken.histogram, 1 / me->sampling_ratio);
    }
}

void
FixedRateShards__print_histogram_as_json(struct FixedRateShards *me)
{
    Olken__print_histogram_as_json(&me->olken);
}

void
FixedRateShards__destroy(struct FixedRateShards *me)
{
    Olken__destroy(&me->olken);
    *me = (struct FixedRateShards){0};
}
