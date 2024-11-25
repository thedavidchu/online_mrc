#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "histogram/histogram.h"
#include "logger/logger.h"
#include "lookup/boost_hash_table.h"
#include "lookup/hash_table.h"
#include "lookup/k_hash_table.h"
#include "lookup/lookup.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "unused/mark_unused.h"

#include "profile/profile.h"

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
        LOGGER_ERROR("cannot initialize tree");
        goto tree_error;
    }
    if (!KHashTable__init(&me->hash_table)) {
        LOGGER_ERROR("cannot initialize hash table");
        goto hash_table_error;
    }
    if (!Histogram__init(&me->histogram,
                         histogram_num_bins,
                         histogram_bin_size,
                         out_of_bounds_mode)) {
        LOGGER_ERROR("cannot initialize histogram");
        goto histogram_error;
    }
#ifdef PROFILE_STATISTICS
    if (!ProfileStatistics__init(&me->prof_stats)) {
        goto histogram_error;
    }
#endif
    me->current_time_stamp = 0;
    return true;

histogram_error:
    KHashTable__destroy(&me->hash_table);
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
    // NOTE We mark these as unused so that my IDE thinks that we are
    //      really using all three hash table libraries.
    // TODO Maybe remove the #includes for the unused hash table libraries.
    UNUSED(BoostHashTable__init);
    UNUSED(HashTable__init);
    UNUSED(KHashTable__init);
    return initialize(me,
                      histogram_num_bins,
                      histogram_bin_size,
                      HistogramOutOfBoundsMode__allow_overflow);
}

bool
Olken__init_full(struct Olken *const me,
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
    bool ok = true;
    size_t size = 0;
    MAYBE_UNUSED(size);
    if (!me) {
        return false;
    }
    size = KHashTable__get_size(&me->hash_table);
    struct LookupReturn r = KHashTable__remove(&me->hash_table, entry);
    if (!r.success) {
        return false;
    }
    assert(KHashTable__get_size(&me->hash_table) + 1 == size);

    size = me->tree.cardinality;
    ok = tree__sleator_remove(&me->tree, r.timestamp);
    assert(me->tree.cardinality + 1 == size);
    return ok;
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
    if (KHashTable__put(&me->hash_table, entry, me->current_time_stamp) !=
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
    if (KHashTable__put(&me->hash_table, entry, me->current_time_stamp) !=
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
Olken__access_item(struct Olken *const me, EntryType const entry)
{
    if (me == NULL) {
        return false;
    }
    struct LookupReturn found = KHashTable__lookup(&me->hash_table, entry);
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
Olken__post_process(struct Olken *const me)
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
Olken__destroy(struct Olken *const me)
{
    if (me == NULL) {
        return;
    }
    tree__destroy(&me->tree);
    KHashTable__destroy(&me->hash_table);
    Histogram__destroy(&me->histogram);
#ifdef PROFILE_STATISTICS
    ProfileStatistics__log(&me->prof_stats, "Olken");
    ProfileStatistics__destroy(&me->prof_stats);
#endif
    *me = (struct Olken){0};
}

bool
Olken__get_histogram(struct Olken const *const me,
                     struct Histogram const **const histogram)
{
    if (me == NULL || histogram == NULL) {
        // NOTE I was going to set '*histogram = NULL' if 'me == NULL',
        //      but this is kind of weird and required some fancy error
        //      handling in case 'histogram == NULL'. I decided to not.
        return false;
    }
    *histogram = &me->histogram;
    return true;
}
