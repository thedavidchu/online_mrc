#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "file/file.h"
#include "histogram/histogram.h"
#include "interval_statistics/interval_statistics.h"
#include "invariants/implies.h"
#include "logger/logger.h"

bool
IntervalStatistics__init(struct IntervalStatistics *const me,
                         size_t const init_capacity)
{
    if (me == NULL || init_capacity == 0) {
        return false;
    }
    *me = (struct IntervalStatistics){
        .stats = calloc(init_capacity, sizeof(*me->stats)),
        .length = 0,
        .capacity = init_capacity};
    return true;
}

static bool
resize(struct IntervalStatistics *const me)
{
    assert(me && me->capacity != 0);
    size_t const new_cap = me->capacity * 1.25;
    struct IntervalStatisticsItem *new_stats =
        realloc(me->stats, new_cap * sizeof(*me->stats));
    if (new_stats == NULL) {
        return false;
    }
    *me = (struct IntervalStatistics){.stats = new_stats,
                                      .capacity = new_cap,
                                      .length = me->length};
    return true;
}

bool
IntervalStatistics__append(struct IntervalStatistics *const me,
                           double const reuse_distance,
                           double const reuse_time)
{
    if (me == NULL) {
        return false;
    }
    if (me->length >= me->capacity) {
        if (!resize(me)) {
            return false;
        }
    }

    // NOTE Yes, I know I could do "me->stats[me->length++]" but many
    //      inexperienced C programmers would not understand what this
    //      means. It's a common idiom in C but I prefer to be clear
    //      even if I must be more verbose.
    me->stats[me->length] =
        (struct IntervalStatisticsItem){.reuse_distance = reuse_distance,
                                        .reuse_time = reuse_time};
    ++me->length;
    return true;
}

bool
IntervalStatistics__save(struct IntervalStatistics const *const me,
                         char const *const path)
{
    if (me == NULL || path == NULL) {
        return false;
    }

    return write_buffer(path, me->stats, me->length, sizeof(*me->stats));
}

void
IntervalStatistics__destroy(struct IntervalStatistics *const me)
{
    if (me == NULL) {
        return;
    }
    free(me->stats);
    *me = (struct IntervalStatistics){0};
}

bool
IntervalStatistics__to_histogram(struct IntervalStatistics const *const me,
                                 struct Histogram *const hist,
                                 size_t const num_bins,
                                 size_t const bin_size)
{
    if (me == NULL || hist == NULL) {
        return false;
    }
    if (!implies(me->length != 0, me->stats != NULL)) {
        LOGGER_ERROR("inconsistent state");
        return false;
    }

    if (!Histogram__init(hist, num_bins, bin_size, false)) {
        return false;
    }

    for (size_t i = 0; i < me->length; ++i) {
        double reuse_dist = me->stats[i].reuse_distance;
        if (isnan(reuse_dist)) {
            // NOTE NAN represents a non-sampled value.
        } else if (isinf(reuse_dist)) {
            Histogram__insert_infinite(hist);
        } else {
            if (reuse_dist > (uint64_t)1 << 53) {
                LOGGER_WARN("lost precision on reuse distance %g", reuse_dist);
            }
            Histogram__insert_finite(hist, (uint64_t)reuse_dist);
        }
    }
    return true;
}
