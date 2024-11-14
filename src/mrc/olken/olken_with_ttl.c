#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "histogram/histogram.h"
#include "logger/logger.h"
#include "lookup/dictionary.h"
#include "lookup/lookup.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "olken/olken.h"
#include "olken/olken_with_ttl.h"
#include "priority_queue/heap.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "unused/mark_unused.h"

size_t const DEFAULT_HEAP_SIZE = 1 << 20;

static bool
initialize(struct OlkenWithTTL *const me,
           size_t const histogram_num_bins,
           size_t const histogram_bin_size,
           enum HistogramOutOfBoundsMode const out_of_bounds_mode,
           struct Dictionary const *const dictionary)
{
    if (me == NULL) {
        LOGGER_WARN("bad input");
        return false;
    }

    if (!Olken__init_full(&me->olken,
                          histogram_num_bins,
                          histogram_bin_size,
                          out_of_bounds_mode)) {
        LOGGER_WARN("failed to initialize Olken");
        goto cleanup;
    }
    if (!Heap__init_min_heap(&me->pq, DEFAULT_HEAP_SIZE)) {
        LOGGER_WARN("failed to init TTL heap");
        goto cleanup;
    }
    me->dictionary = dictionary;
    return true;
cleanup:
    OlkenWithTTL__destroy(me);
    return false;
}

bool
OlkenWithTTL__init(struct OlkenWithTTL *const me,
                   size_t const histogram_num_bins,
                   size_t const histogram_bin_size)
{
    return initialize(me,
                      histogram_num_bins,
                      histogram_bin_size,
                      HistogramOutOfBoundsMode__allow_overflow,
                      NULL);
}

bool
OlkenWithTTL__init_full(struct OlkenWithTTL *const me,
                        size_t const histogram_num_bins,
                        size_t const histogram_bin_size,
                        enum HistogramOutOfBoundsMode const out_of_bounds_mode,
                        struct Dictionary const *const dictionary)
{
    return initialize(me,
                      histogram_num_bins,
                      histogram_bin_size,
                      out_of_bounds_mode,
                      dictionary);
}

/// @brief  Evict all data that expires before current timestamp.
/// @note   We do not evict the item until its expiry time has passed!
///         That means we still track it up to its expiry time.
static void
evict_expired_items(struct OlkenWithTTL *const me, TimeStampType current_time)
{
    assert(me != NULL);
    while (true) {
        bool r = true;
        KeyType oldest_expiry_time = Heap__get_top_key(&me->pq);
        EntryType rm_entry = 0;
        if (oldest_expiry_time >= current_time) {
            break;
        }
        r = Heap__remove(&me->pq, oldest_expiry_time, &rm_entry);
        assert(r);
        Olken__remove_item(&me->olken, rm_entry);
    }
}

static bool
update_item(struct OlkenWithTTL *me, EntryType entry, TimeStampType timestamp)
{
    uint64_t distance = Olken__update_stack(&me->olken, entry, timestamp);
    if (distance == UINT64_MAX) {
        return false;
    }
    Histogram__insert_finite(&me->olken.histogram, distance);
    return true;
}

static bool
insert_item(struct OlkenWithTTL *const me,
            EntryType const entry,
            TimeStampType const eviction_time)
{
    if (!Heap__insert(&me->pq, entry, eviction_time)) {
        LOGGER_ERROR("TTL heap insertion failed");
        return false;
    }
    if (!Olken__insert_stack(&me->olken, entry)) {
        LOGGER_ERROR("Olken insertion failed");
        return false;
    }
    Histogram__insert_infinite(&me->olken.histogram);
    return true;
}

bool
OlkenWithTTL__access_item(struct OlkenWithTTL *const me,
                          EntryType const entry,
                          TimeStampType const timestamp,
                          TimeStampType const ttl)
{
    // NOTE I use the nullness of the hash table as a proxy for whether this
    //      data structure has been initialized.
    if (me == NULL) {
        return false;
    }
    evict_expired_items(me, timestamp);
    struct LookupReturn r = Olken__lookup(&me->olken, entry);
    if (r.success) {
        bool ok = update_item(me, entry, r.timestamp);
        return ok;
    } else {
        bool ok = insert_item(me, entry, timestamp + ttl);
        return ok;
    }
}

bool
OlkenWithTTL__post_process(struct OlkenWithTTL *me)
{
    UNUSED(me);
    return true;
}

bool
OlkenWithTTL__to_mrc(struct OlkenWithTTL const *const me,
                     struct MissRateCurve *const mrc)
{
    return Olken__to_mrc(&me->olken, mrc);
}

void
OlkenWithTTL__print_histogram_as_json(struct OlkenWithTTL *me)
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
OlkenWithTTL__destroy(struct OlkenWithTTL *me)
{
    Olken__destroy(&me->olken);
    Heap__destroy(&me->pq);
    *me = (struct OlkenWithTTL){0};
}

bool
OlkenWithTTL__get_histogram(struct OlkenWithTTL *const me,
                            struct Histogram const **const histogram)
{
    if (me == NULL) {
        return false;
    }
    return Olken__get_histogram(&me->olken, histogram);
}
