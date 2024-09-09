#pragma once

#include <immintrin.h>
#include <stdint.h>
#include <x86intrin.h>

/// @note   I admit the semantics are confusing.
static inline uint64_t
start_rdtsc(void)
{
    return __rdtsc();
}

/// @note   I admit the semantics are confusing.
static inline uint64_t
end_rdtsc(uint64_t const start)
{
    return __rdtsc() - start;
}
