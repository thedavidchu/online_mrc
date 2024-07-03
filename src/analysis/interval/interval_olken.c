#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "interval/interval_olken.h"
#include "interval_statistics/interval_statistics.h"
#include "logger/logger.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "olken/olken.h"

bool
IntervalOlken__init(struct IntervalOlken *me, size_t const length)
{
    if (me == NULL) {
        LOGGER_ERROR("invalid me");
        return false;
    }
    // NOTE I set the number of bins and bin size to 1 because I was
    //      lazy and made values of 0 return an error.
    if (!Olken__init(&me->olken, 1, 1)) {
        LOGGER_ERROR("failed to initialize Olken");
        goto cleanup;
    }
    if (!IntervalStatistics__init(&me->stats, length)) {
        goto cleanup;
    }
    return true;
cleanup:
    IntervalOlken__destroy(me);
    return false;
}

void
IntervalOlken__destroy(struct IntervalOlken *me)
{
    Olken__destroy(&me->olken);
    IntervalStatistics__destroy(&me->stats);
    *me = (struct IntervalOlken){0};
}

bool
IntervalOlken__access_item(struct IntervalOlken *me, EntryType const entry)
{
    size_t reuse_dist = 0, reuse_time = 0;

    if (me == NULL) {
        return false;
    }

    struct LookupReturn found = HashTable__lookup(&me->olken.hash_table, entry);
    if (found.success) {
        reuse_time = me->olken.current_time_stamp - found.timestamp - 1;
        reuse_dist = Olken__update_stack(&me->olken, entry, found.timestamp);
        if (reuse_dist == UINT64_MAX) {
            return false;
        }
    } else {
        reuse_dist = SIZE_MAX;
        reuse_time = SIZE_MAX;
        if (!Olken__insert_stack(&me->olken, entry)) {
            return false;
        }
    }
    // Insert reuse_dist and reuse_time statistics.
    if (!IntervalStatistics__append(&me->stats, reuse_dist, reuse_time)) {
        return false;
    }
    return true;
}

bool
IntervalOlken__write_results(struct IntervalOlken *me, char const *const path)
{
    return IntervalStatistics__save(&me->stats, path);
}
