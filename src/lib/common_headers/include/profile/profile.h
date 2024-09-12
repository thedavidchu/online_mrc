/** @brief  Functions to start and end the cycle counter. If the macro
 *          'PROFILE_STATISTICS' is not defined, then I try to make all
 *          of these no-ops. This was to make the files I'm profiling
 *          cleaner.
 */
#pragma once

#include <immintrin.h>
#include <stdint.h>
#include <x86intrin.h>

/// @note   I admit the semantics are confusing.
static inline uint64_t
start_tick_counter(void)
{
#ifdef PROFILE_STATISTICS
    return __rdtsc();
#else
    return 0;
#endif
}

/// @note   I admit the semantics are confusing.
static inline uint64_t
end_tick_counter(uint64_t const start)
{
#ifdef PROFILE_STATISTICS
    return __rdtsc() - start;
#else
    UNUSED(start);
    return 0;
#endif
}

#ifdef PROFILE_STATISTICS
#define UPDATE_TICK_COUNTER(start, tick_counter_ptr, hit_counter_ptr)          \
    do {                                                                       \
        (*(tick_counter_ptr)) += end_tick_counter(start);                      \
        ++(*(hit_counter_ptr));                                                \
    } while (0)
#else
#define UPDATE_TICK_COUNTER(start, tick_counter_ptr, hit_counter_ptr) ((void)0)
#endif
