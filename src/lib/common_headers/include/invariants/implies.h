#pragma once

#include <stdbool.h>

/// @brief  Boolean logic for x => y
/// @note   Here is the truth table for implications:
///             False => False : Implication holds
///             False => True  : Implication holds
///             True  => False : Implication does NOT hold
///             True  => True  : Implication holds
///         Therefore, the implication holds if x is False or both x
///         and y are true. Alternatively, we could represent it as
///         follows: `x ? y : true`
static inline bool
implies(bool const x, bool const y)
{
    return x ? y : true;
}
