#pragma once

#include <stdbool.h>
#include <stdint.h>

/// @note   We exclude '0' because it is not a power of two, at least
///         for finite powers. 0 = 2^-inf.
///         Source:
///         https://stackoverflow.com/questions/600293/how-to-check-if-a-number-is-a-power-of-2
static inline bool
is_power_of_two(uint64_t const x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}
