/// @brief  This file implements saturated arithmetic.
#pragma once

#include <stddef.h>
#include <stdint.h>

/// Source:
/// https://stackoverflow.com/questions/199333/how-do-i-detect-unsigned-integer-overflow
static inline size_t
saturation_add(size_t const a, size_t const b)
{
    // NOTE a >= 0 because we're using unsigned integers.
    if (a > SIZE_MAX - b) {
        return SIZE_MAX;
    }
    return a + b;
}
