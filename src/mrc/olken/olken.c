#include <assert.h>
#include <stdbool.h>
#include <stdint.h> // uint64_t
#include <stdio.h>
#include <stdlib.h>

#include "histogram/histogram.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

static bool
initialize(struct Olken *const me,
           size_t const histogram_num_bins,
           size_t const histogram_bin_size,
           enum HistogramOutOfBoundsMode const out_of_bounds_mode)
{
    if (me == NULL) {
        return false;
    }
    if (!tree__init(&me->tree)) {
        goto tree_error;
    }
    if (!HashTable__init(&me->hash_table)) {
        goto hash_table_error;
    }
    if (!Histogram__init(&me->histogram,
                         histogram_num_bins,
                         histogram_bin_size,
                         out_of_bounds_mode)) {
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

bool
Olken__init(struct Olken *const me,
            size_t const histogram_num_bins,
            size_t const histogram_bin_size)
{
    return initialize(me,
                      histogram_num_bins,
                      histogram_bin_size,
                      HistogramOutOfBoundsMode__allow_overflow);
}

bool
Olken__init_full(struct Olken *me,
                 size_t const histogram_num_bins,
                 size_t const histogram_bin_size,
                 enum HistogramOutOfBoundsMode const out_of_bounds_mode)
{
    return initialize(me,
                      histogram_num_bins,
                      histogram_bin_size,
                      out_of_bounds_mode);
}
bool
Olken__remove_item(struct Olken *me, EntryType entry)
{
    struct LookupReturn r = HashTable__remove(&me->hash_table, entry);
    if (!r.success) {
        return false;
    }
    return tree__sleator_remove(&me->tree, r.timestamp);
}

void
Olken__ignore_entry(struct Olken *me)
{
    ++me->current_time_stamp;
}

/// @return Return the stack distance of an existing item or uint64::MAX
///         upon an error.
uint64_t
Olken__update_stack(struct Olken *me, EntryType entry, TimeStampType timestamp)
{
    if (me == NULL) {
        return UINT64_MAX;
    }
    uint64_t distance = tree__reverse_rank(&me->tree, timestamp);
    if (!tree__sleator_remove(&me->tree, timestamp)) {
        return UINT64_MAX;
    }
    if (!tree__sleator_insert(&me->tree, me->current_time_stamp)) {
        return UINT64_MAX;
    }
    if (HashTable__put_unique(&me->hash_table, entry, me->current_time_stamp) !=
        LOOKUP_PUTUNIQUE_REPLACE_VALUE) {
        return UINT64_MAX;
    }
    ++me->current_time_stamp;
    return distance;
}

bool
Olken__insert_stack(struct Olken *me, EntryType entry)
{
    if (me == NULL) {
        return false;
    }
    if (HashTable__put_unique(&me->hash_table, entry, me->current_time_stamp) !=
        LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE) {
        return false;
    }
    if (!tree__sleator_insert(&me->tree, me->current_time_stamp)) {
        return false;
    }
    ++me->current_time_stamp;
    return true;
}

bool
Olken__access_item(struct Olken *me, EntryType entry)
{
    if (me == NULL) {
        return false;
    }

    struct LookupReturn found = HashTable__lookup(&me->hash_table, entry);
    if (found.success) {
        uint64_t distance = Olken__update_stack(me, entry, found.timestamp);
        if (distance == UINT64_MAX) {
            return false;
        }
        // TODO(dchu): Maybe record the infinite distances for Parda!
        Histogram__insert_finite(&me->histogram, distance);
    } else {
        if (!Olken__insert_stack(me, entry)) {
            return false;
        }
        Histogram__insert_infinite(&me->histogram);
    }

    return true;
}

bool
Olken__post_process(struct Olken *me)
{
    UNUSED(me);
    return true;
}

bool
Olken__to_mrc(struct Olken const *const me, struct MissRateCurve *const mrc)
{
    return MissRateCurve__init_from_histogram(mrc, &me->histogram);
}

void
Olken__print_histogram_as_json(struct Olken *me)
{
    if (me == NULL) {
        // Just pass on the NULL value and let the histogram deal
        // with it. Maybe this isn't very smart and will confuse
        // future-me? Oh well!
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

bool
Olken__get_histogram(struct Olken const *const me,
                     struct Histogram const *histogram)
{
    if (me == NULL) {
        histogram = NULL;
        return false;
    }
    histogram = &me->histogram;
    return true;
}
