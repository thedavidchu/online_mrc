#pragma once

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stddef.h>

/// @brief  Calculate if we are on the N-th iteration.
static inline bool
is_nth_iter(size_t const i, size_t const n)
{
    return (i + 1) % n == 0;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
