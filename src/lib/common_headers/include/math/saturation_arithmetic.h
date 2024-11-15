/// @brief  This file implements saturated arithmetic.
/// Source:
/// https://stackoverflow.com/questions/199333/how-do-i-detect-unsigned-integer-overflow
#pragma once

#include <stddef.h>
#include <stdint.h>

static inline size_t
saturation_add(size_t const a, size_t const b)
{
    // NOTE a >= 0 because we're using unsigned integers.
    if (a > SIZE_MAX - b) {
        return SIZE_MAX;
    }
    return a + b;
}

static inline size_t
saturation_multiply(size_t const a, size_t const b)
{
    // I do not need to check for negative numbers, since I'm working
    // with non-negative unsigned integers.
    // NOTE If b == 0, then the result will be correct, since a * 0 = 0.
    if (b != 0 && a > SIZE_MAX / b) {
        return SIZE_MAX;
    }
    return a * b;
}
