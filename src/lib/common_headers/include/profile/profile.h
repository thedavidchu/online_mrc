/** @brief  Track the number of cycles and hits for an area of code. If
 *          'PROFILE_STATISTICS' is not defined, then I try to make the
 *          helper function 'start_tick_counter()' and macro
 *          'UPDATE_PROFILE_STATISTICS()' into no-ops. Functions that
 *          require the 'struct ProfileStatistics' parameter must be
 *          enclosed in '#ifdef'/'#endif'.
 */
#pragma once

#include <immintrin.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <x86intrin.h>

#include "logger/logger.h"
#include "unused/mark_unused.h"

struct ProfileStatistics {
    // Number of "timestamps" (i.e. clock-cycles, sort of...)
    uint64_t tsc_counter;
    // Number of invocations
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

static inline bool
ProfileStatistics__log(struct ProfileStatistics const *const me,
                       char const *const msg)
{
    if (me == NULL) {
        return false;
    }
    LOGGER_INFO(
        "'%s' profile statistics -- TSC Count: %" PRIu64
        " | Hit Count: %" PRIu64 " | Average TSC per Hit: %f",
        msg ? msg : "unlabelled",
        me->tsc_counter,
        me->hit_counter,
        me->hit_counter == 0 ? NAN : (double)me->tsc_counter / me->hit_counter);
    return true;
}

static inline void
ProfileStatistics__destroy(struct ProfileStatistics *const me)
{
    if (me == NULL) {
        return;
    }
    *me = (struct ProfileStatistics){0};
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
        ProfileStatistics__update((prof_stat_ptr), (start));                   \
    } while (0)
#else
// NOTE We allow the 'start' variable to exist when PROFILE_STATISTICS
//      isn't defined, but the actual counters will not exist.
#define UPDATE_PROFILE_STATISTICS(prof_stat_ptr, start) UNUSED(start)
#endif
