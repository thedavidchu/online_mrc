#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hash/MyMurmurHash3.h"
#include "hash/types.h"
#include "histogram/histogram.h"
#include "logger/logger.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "math/ratio.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "shards/fixed_rate_shards.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "types/entry_type.h"

bool
FixedRateShards__init(struct FixedRateShards *me,
                      const double sampling_ratio,
                      const uint64_t histogram_num_bins,
                      const uint64_t histogram_bin_size,
                      const bool adjustment)
{
    if (me == NULL || sampling_ratio <= 0.0 || 1.0 < sampling_ratio)
        return false;
    // NOTE I am assuming that Olken does not have any structures that
    //      point to the containing structure (i.e. the 'shell' of the
    //      Olken structure is not referenced anywhere).
    bool r = Olken__init(&me->olken, histogram_num_bins, histogram_bin_size);
    if (!r)
        return false;
    me->sampling_ratio = sampling_ratio;
    me->threshold = ratio_uint64(sampling_ratio);
    me->scale = 1 / sampling_ratio;

    me->adjustment = adjustment;
    me->num_entries_seen = 0;
    me->num_entries_processed = 0;
    return true;
}

void
FixedRateShards__access_item(struct FixedRateShards *me, EntryType entry)
{
    bool r = false;

    if (me == NULL) {
        return;
    }

    ++me->num_entries_seen;
    Hash64BitType hash = Hash64bit(entry);
    // NOTE Taking the modulo of the hash by 1 << 24 reduces the accuracy
    //      significantly. I tried dividing the threshold by 1 << 24 and also
    //      leaving the threshold alone. Neither worked to improve accuracy.
    if (hash > me->threshold)
        return;
    ++me->num_entries_processed;

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
        Histogram__insert_scaled_finite(&me->olken.histogram,
                                        distance,
                                        me->scale);
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
        Histogram__insert_scaled_infinite(&me->olken.histogram, me->scale);
    }
}

void
FixedRateShards__post_process(struct FixedRateShards *me)
{
    if (me == NULL || me->olken.histogram.histogram == NULL ||
        me->olken.histogram.num_bins < 1)
        return;

    if (!me->adjustment)
        return;

    // NOTE I need to scale the adjustment by the scale that I've been adjusting
    //      all values. Conversely, I could just not scale any values by the
    //      scale and I'd be equally well off (in fact, better probably,
    //      because a smaller chance of overflowing).
    const int64_t adjustment =
        me->scale *
        (me->num_entries_seen * me->sampling_ratio - me->num_entries_processed);
    bool r = Histogram__adjust_first_buckets(&me->olken.histogram, adjustment);
    if (!r) {
        LOGGER_WARN("error in adjusting buckets");
    }
}

bool
FixedRateShards__to_mrc(struct FixedRateShards const *const me,
                        struct MissRateCurve *const mrc)
{
    return MissRateCurve__init_from_histogram(mrc, &me->olken.histogram);
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
