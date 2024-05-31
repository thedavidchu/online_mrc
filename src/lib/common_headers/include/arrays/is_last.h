#pragma once

#include <stdbool.h>
#include <stddef.h>

static inline bool
is_last(size_t const idx, size_t const length)
{
    return idx == length - 1;
}
