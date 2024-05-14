#include <assert.h>
#include <stdbool.h> // bool
#include <stdint.h>  // uint64_t
#include <stdio.h>
#include <stdlib.h>

#include "histogram/histogram.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "olken/olken.h"
#include "sampling/reservoir_sampling.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "tree/types.h"

#include "reservoir_sampling_mrc/reservoir_sampling_mrc.h"

bool
ReservoirSamplingMRC__init(struct ReservoirSamplingMRC *me,
                           const uint64_t max_num_unique_entries,
                           const uint64_t histogram_bin_size)
{
    bool r = false;
    if (me == NULL) {
        return false;
    }
    r = ReservoirSamplingAlgorithmR__init(&me->reservoir, 1 << 13, 0);
    if (!r) {
        goto cleanup;
    }
    r = Olken__init(&me->olken, max_num_unique_entries, histogram_bin_size);
    if (!r) {
        goto cleanup;
    }
    return true;

cleanup:
    ReservoirSamplingAlgorithmR__destroy(&me->reservoir);
    return false;
}

void
ReservoirSamplingMRC__access_item(struct ReservoirSamplingMRC *me,
                                  EntryType entry)
{
    bool r = false;

    if (me == NULL) {
        return;
    }

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
        Histogram__insert_finite(&me->olken.histogram, distance);
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
        Histogram__insert_infinite(&me->olken.histogram);
    }
}

void
ReservoirSamplingMRC__print_histogram_as_json(struct ReservoirSamplingMRC *me)
{
    if (me == NULL) {
        // Just pass on the NULL value and let the histogram deal with it. Maybe
        // this isn't very smart and will confuse future-me? Oh well!
        Histogram__print_as_json(NULL);
        return;
    }
    Histogram__print_as_json(&me->olken.histogram);
}

void
ReservoirSamplingMRC__destroy(struct ReservoirSamplingMRC *me)
{
    if (me == NULL) {
        return;
    }
    Olken__destroy(&me->olken);
    ReservoirSamplingAlgorithmR__destroy(&me->reservoir);
    *me = (struct ReservoirSamplingMRC){0};
}
