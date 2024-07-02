#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "file/file.h"
#include "interval/interval_statistics.h"

bool
ReuseStatistics__init(struct ReuseStatistics *const me,
                      size_t const init_capacity)
{
    if (me == NULL || init_capacity == 0) {
        return false;
    }
    *me = (struct ReuseStatistics){
        .stats = calloc(init_capacity, sizeof(*me->stats)),
        .length = 0,
        .capacity = init_capacity};
    return true;
}

static bool
resize(struct ReuseStatistics *const me)
{
    assert(me && me->capacity != 0);
    size_t const new_cap = me->capacity * 1.25;
    struct ReuseStatisticsItem *new_stats =
        realloc(me->stats, new_cap * sizeof(*me->stats));
    if (new_stats == NULL) {
        return false;
    }
    *me = (struct ReuseStatistics){.stats = new_stats,
                                   .capacity = new_cap,
                                   .length = me->length};
    return true;
}

bool
ReuseStatistics__append(struct ReuseStatistics *const me,
                        uint64_t const reuse_distance,
                        uint64_t const reuse_time)
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
        (struct ReuseStatisticsItem){.reuse_distance = reuse_distance,
                                     .reuse_time = reuse_time};
    ++me->length;
    return true;
}

bool
ReuseStatistics__save(struct ReuseStatistics const *const me,
                      char const *const path)
{
    if (me == NULL || path == NULL) {
        return false;
    }

    return write_buffer(path, me->stats, me->length, sizeof(*me->stats));
}

void
ReuseStatistics__destroy(struct ReuseStatistics *const me)
{
    if (me == NULL) {
        return;
    }
    free(me->stats);
    *me = (struct ReuseStatistics){0};
}
