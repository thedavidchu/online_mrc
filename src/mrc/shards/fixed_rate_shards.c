#include <stdint.h>
#include <string.h>

#include "hash/splitmix64.h"
#include "hash/types.h"
#include "olken/olken.h"
#include "shards/fixed_rate_shards.h"
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
    Hash64BitType hash = splitmix64_hash(entry);
    if (hash > me->threshold)
        return;
    Olken__access_item(&me->olken, entry);
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
