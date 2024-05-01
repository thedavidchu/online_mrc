#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "hash/splitmix64.h"
#include "hash/types.h"
#include "olken/olken.h"
#include "lookup/hash_table.h"
#include "shards/fixed_rate_shards.h"
#include "tree/sleator_tree.h"
#include "tree/basic_tree.h"
#include "types/entry_type.h"

bool
FixedRateShards__init(struct FixedRateShards *me,
                      const uint64_t max_num_unique_entries,
                      const uint64_t threshold)
{
    // NOTE I am assuming that Olken does not have any structures that
    //      point to the containing structure (i.e. the 'shell' of the
    //      Olken structure is not referenced anywhere).
    bool r = Olken__init(&me->olken, max_num_unique_entries);
    if (!r)
        return false;
    me->threshold = threshold;
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
    if (hash > me->threshold)
        return;

    struct LookupReturn found =
        HashTable__lookup(&me->olken.hash_table, entry);
    if (found.success) {
        uint64_t distance =
            tree__reverse_rank(&me->olken.tree, (KeyType)found.timestamp);
        r = tree__sleator_remove(&me->olken.tree, (KeyType)found.timestamp);
        assert(r && "remove should not fail");
        r = tree__sleator_insert(&me->olken.tree,
                                 (KeyType)me->olken.current_time_stamp);
        assert(r && "insert should not fail");
        r = HashTable__put_unique(&me->olken.hash_table,
                                          entry,
                                          me->olken.current_time_stamp);
        assert(r && "update should not fail");
        ++me->olken.current_time_stamp;
        // TODO(dchu): Maybe record the infinite distances for Parda!
        basic_histogram__insert_finite(&me->olken.histogram, distance);
    } else {
        r = HashTable__put_unique(&me->olken.hash_table,
                                          entry,
                                          me->olken.current_time_stamp);
        assert(r && "insert should not fail");
        tree__sleator_insert(&me->olken.tree, (KeyType)me->olken.current_time_stamp);
        ++me->olken.current_time_stamp;
        basic_histogram__insert_infinite(&me->olken.histogram);
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
