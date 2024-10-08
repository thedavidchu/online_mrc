#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "interval/interval_olken.h"
#include "interval_statistics/interval_statistics.h"
#include "logger/logger.h"
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
    double reuse_dist = 0, reuse_time = 0;

    if (me == NULL) {
        return false;
    }

    struct LookupReturn found = Olken__lookup(&me->olken, entry);
    if (found.success) {
        uint64_t rt = me->olken.current_time_stamp - found.timestamp - 1;
        uint64_t rd = Olken__update_stack(&me->olken, entry, found.timestamp);
        if (reuse_dist == UINT64_MAX) {
            return false;
        }
        if (rt > (uint64_t)1 << 53) {
            LOGGER_WARN("losing precision on reuse time %g", rt);
        }
        if (rd > (uint64_t)1 << 53) {
            LOGGER_WARN("losing precision on reuse distance %g", rd);
        }
        reuse_time = (double)rt;
        reuse_dist = (double)rd;
    } else {
        reuse_dist = INFINITY;
        reuse_time = INFINITY;
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
