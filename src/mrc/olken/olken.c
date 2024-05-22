#include <assert.h>
#include <stdbool.h> // bool
#include <stdint.h>  // uint64_t
#include <stdio.h>
#include <stdlib.h>

#include "histogram/histogram.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "olken/olken.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "tree/types.h"

bool
Olken__init(struct Olken *me,
            const uint64_t histogram_num_bins,
            const uint64_t histogram_bin_size)
{
    if (me == NULL) {
        return false;
    }
    bool r = tree__init(&me->tree);
    if (!r) {
        goto tree_error;
    }
    r = HashTable__init(&me->hash_table);
    if (!r) {
        goto hash_table_error;
    }
    r = Histogram__init(&me->histogram, histogram_num_bins, histogram_bin_size);
    if (!r) {
        goto histogram_error;
    }
    me->current_time_stamp = 0;
    return true;

histogram_error:
    HashTable__destroy(&me->hash_table);
hash_table_error:
    tree__destroy(&me->tree);
tree_error:
    return false;
}

void
Olken__access_item(struct Olken *me, EntryType entry)
{
    bool r = false;

    if (me == NULL) {
        return;
    }

    struct LookupReturn found = HashTable__lookup(&me->hash_table, entry);
    if (found.success) {
        uint64_t distance =
            tree__reverse_rank(&me->tree, (KeyType)found.timestamp);
        r = tree__sleator_remove(&me->tree, (KeyType)found.timestamp);
        assert(r && "remove should not fail");
        r = tree__sleator_insert(&me->tree, (KeyType)me->current_time_stamp);
        assert(r && "insert should not fail");
        enum PutUniqueStatus s = HashTable__put_unique(&me->hash_table,
                                                       entry,
                                                       me->current_time_stamp);
        assert(s == LOOKUP_PUTUNIQUE_REPLACE_VALUE &&
               "update should replace value");
        ++me->current_time_stamp;
        // TODO(dchu): Maybe record the infinite distances for Parda!
        Histogram__insert_finite(&me->histogram, distance);
    } else {
        enum PutUniqueStatus s = HashTable__put_unique(&me->hash_table,
                                                       entry,
                                                       me->current_time_stamp);
        assert(s == LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE &&
               "update should insert key/value");
        tree__sleator_insert(&me->tree, (KeyType)me->current_time_stamp);
        ++me->current_time_stamp;
        Histogram__insert_infinite(&me->histogram);
    }
}

void
Olken__print_histogram_as_json(struct Olken *me)
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
Olken__destroy(struct Olken *me)
{
    if (me == NULL) {
        return;
    }
    tree__destroy(&me->tree);
    HashTable__destroy(&me->hash_table);
    Histogram__destroy(&me->histogram);
    *me = (struct Olken){0};
}
