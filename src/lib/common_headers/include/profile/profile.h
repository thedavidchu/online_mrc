/** @brief  Functions to start and end the cycle counter. If the macro
 *          'PROFILE_STATISTICS' is not defined, then I try to make all
 *          of these no-ops. This was to make the files I'm profiling
 *          cleaner.
 */
#pragma once

#include <immintrin.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <x86intrin.h>

#include "logger/logger.h"
#include "unused/mark_unused.h"

struct ProfileStatistics {
    uint64_t tsc_counter;
    uint64_t hit_counter;
};

static inline bool
ProfileStatistics__init(struct ProfileStatistics *const me)
{
    if (me == NULL)
        return false;
    *me = (struct ProfileStatistics){.tsc_counter = 0, .hit_counter = 0};
    return true;
}

static inline void
ProfileStatistics__destroy(struct ProfileStatistics *const me)
{
    if (me == NULL)
        return;
#ifdef PROFILE_STATISTICS
    ProfileStatistics__destroy(&me->prof_stats);
#endif
}

/// @note   I admit the semantics are confusing.
static inline uint64_t
start_tick_counter(void)
{
    uint64_t start = 0;
#ifdef PROFILE_STATISTICS
    start = __rdtsc();
#endif
    return start;
}

static inline bool
ProfileStatistics__update(struct ProfileStatistics *const me,
                          uint64_t const start)
{
#ifdef PROFILE_STATISTICS
    if (me == NULL)
        return false;
    me->tsc_counter += __rdtsc() - start;
    ++me->hit_counter;
#endif
    MAYBE_UNUSED(me);
    MAYBE_UNUSED(start);
    return true;
}

#ifdef PROFILE_STATISTICS
#define UPDATE_PROFILE_STATISTICS(prof_stat_ptr, start)                        \
    do {                                                                       \
        ProfileStatistics__update((prof_stat_ptr), (start))                    \
    } while (0)
#else
// NOTE We allow the 'start' variable to exist when PROFILE_STATISTICS
//      isn't defined, but the actual counters will not exist.
#define UPDATE_PROFILE_STATISTICS(prof_stat_ptr, start) UNUSED(start)
#endif
