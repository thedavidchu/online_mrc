#pragma once

#include <stdint.h>

/// @brief  Compute the ratio of the maximum uint64_t value.
/// @note   My implementations tend to struggle with overflow if the
///         ratio is 1.0; this is an attempt to ameliorate that by
///         checking for (some) overflow conditions.
static inline uint64_t
ratio_uint64(const double ratio)
{
    const uint64_t threshold = ratio * UINT64_MAX;
    // NOTE If init_sampling_ratio == 1.0, then it causes the
    //      threshold to overflow and become zero. Therefore, if the
    //      threshold is less than the ratio, we assume we overflowed.
    //      I'm either very clever or this is buggy because I haven't
    //      tested it. :D
    return threshold < ratio ? UINT64_MAX : threshold;
}
